#include "TestHarness.h"

#include <string>

#include "src/network/IXiaozhiTransport.h"
#include "src/network/XiaozhiBootstrapClient.h"
#include "src/network/XiaozhiSession.h"

using smv::network::BootstrapError;
using smv::network::BootstrapParser;
using smv::network::BootstrapResult;
using smv::network::ActivationPollStatus;
using smv::network::ICloudSessionGuard;
using smv::network::IXiaozhiTransport;
using smv::network::IXiaozhiTransportListener;
using smv::network::SessionState;
using smv::network::TransportConfig;
using smv::network::XiaozhiSession;

namespace {

constexpr const char* VALID_BOOTSTRAP = R"({
  "websocket":{"url":"wss://api.example/xiaozhi/v1/","token":"token-1","version":2},
  "activation":{"message":"activate","code":"123456","challenge":"abc","timeout_ms":30000},
  "server_time":{"timestamp":1770000000000,"timezone_offset":420}
})";

class FakeGuard final : public ICloudSessionGuard {
 public:
  void invalidateCloudContext() override { ++invalidations; }
  int invalidations = 0;
};

class FakeTransport final : public IXiaozhiTransport {
 public:
  void setListener(IXiaozhiTransportListener* listener) override {
    listener_ = listener;
  }
  bool connect(const TransportConfig&) override { return true; }
  void disconnect() override {}
  void process() override {}
  bool sendText(std::string_view text) override {
    lastText.assign(text);
    return true;
  }
  bool sendBinary(const uint8_t*, std::size_t) override { return true; }
  bool isConnected() const override { return true; }

  IXiaozhiTransportListener* listener_ = nullptr;
  std::string lastText;
};

}  // namespace

TEST_CASE("bootstrap requires HTTPS WSS and a supported protocol") {
  const BootstrapResult valid = BootstrapParser::parse(VALID_BOOTSTRAP);
  REQUIRE(valid.ok());
  REQUIRE(valid.config.protocolVersion == 2);
  REQUIRE(valid.config.url == "wss://api.example/xiaozhi/v1/");
  REQUIRE(valid.activation.code == "123456");

  REQUIRE(BootstrapParser::parse(
              R"({"websocket":{"url":"ws://plain.example","token":"x","version":2}})")
              .error == BootstrapError::InsecureEndpoint);
  REQUIRE(BootstrapParser::parse(
              R"({"websocket":{"url":"wss://secure.example","token":"x","version":9}})")
              .error == BootstrapError::UnsupportedProtocol);
}

TEST_CASE("bootstrap matches upstream default protocol and activation statuses") {
  const auto withoutVersion = BootstrapParser::parse(
      R"({"websocket":{"url":"wss://api.xiaozhi.me/xiaozhi/v1/","token":"test-token"},"activation":{"code":"654321","challenge":"aa:bb"}})");
  REQUIRE(withoutVersion.ok());
  REQUIRE(withoutVersion.config.protocolVersion == 1);
  REQUIRE(withoutVersion.activation.code == "654321");

  REQUIRE(smv::network::XiaozhiBootstrapClient::classifyActivationHttpStatus(200) ==
          ActivationPollStatus::Activated);
  REQUIRE(smv::network::XiaozhiBootstrapClient::classifyActivationHttpStatus(202) ==
          ActivationPollStatus::Pending);
  REQUIRE(smv::network::XiaozhiBootstrapClient::classifyActivationHttpStatus(401) ==
          ActivationPollStatus::Failed);
  REQUIRE(smv::network::XiaozhiBootstrapClient::activationHttpTimeoutMs(30000) ==
          35000);
  REQUIRE(smv::network::XiaozhiBootstrapClient::activationHttpTimeoutMs(0) ==
          35000);
  REQUIRE(smv::network::XiaozhiBootstrapClient::activationHttpTimeoutMs(600000) ==
          65000);
}

TEST_CASE("bootstrap accepts activation-only provisioning responses") {
  const auto activationOnly = BootstrapParser::parse(
      R"({"activation":{"message":"Open xiaozhi.me","code":"654321","challenge":"challenge-1","timeout_ms":30000}})");
  REQUIRE(activationOnly.ok());
  REQUIRE(activationOnly.activation.required());
  REQUIRE(activationOnly.activation.code == "654321");
  REQUIRE(activationOnly.config.url.empty());
  REQUIRE(BootstrapParser::parse(
              R"({"activation":{"code":"654321"}})")
              .error == BootstrapError::MissingField);
  REQUIRE(BootstrapParser::parse(
              R"({"activation":{"challenge":"challenge-1"}})")
              .error == BootstrapError::MissingField);
}

TEST_CASE("bootstrap rejects malformed oversized and incomplete responses") {
  REQUIRE(BootstrapParser::parse("not-json").error == BootstrapError::MalformedJson);
  REQUIRE(BootstrapParser::parse(std::string(16385, 'x')).error ==
          BootstrapError::ResponseTooLarge);
  REQUIRE(BootstrapParser::parse(R"({"websocket":{"url":"wss://x"}})")
              .error == BootstrapError::MissingField);
}

TEST_CASE("disconnect invalidates candidate and confirmation context") {
  FakeTransport transport;
  FakeGuard guard;
  XiaozhiSession session(transport, guard);
  TransportConfig config{"wss://api.example/path", "token", 2,
                         "AA:BB:CC:DD:EE:FF", "client-uuid", "root-ca"};
  REQUIRE(session.open(config));
  session.onTransportConnected();
  REQUIRE(session.state() == SessionState::AwaitingHello);
  REQUIRE(transport.lastText.find("\"mcp\":true") != std::string::npos);
  REQUIRE(transport.lastText.find("\"sample_rate\":16000") !=
          std::string::npos);

  session.onTransportText(
      R"({"type":"hello","transport":"websocket","session_id":"s-1","audio_params":{"sample_rate":24000,"frame_duration":60}})");
  REQUIRE(session.state() == SessionState::Ready);
  REQUIRE(session.downlinkSampleRate() == 24000);
  REQUIRE(session.downlinkFrameDurationMs() == 60);

  session.onTransportDisconnected();
  REQUIRE(session.state() == SessionState::Disconnected);
  REQUIRE(guard.invalidations == 1);
}

TEST_CASE("invalid hello fails closed and disconnects session") {
  FakeTransport transport;
  FakeGuard guard;
  XiaozhiSession session(transport, guard);
  TransportConfig config{"wss://api.example/path", "token", 2,
                         "device", "client", "root-ca"};
  REQUIRE(session.open(config));
  session.onTransportConnected();
  session.onTransportText(
      R"({"type":"hello","transport":"mqtt","audio_params":{"sample_rate":999999,"frame_duration":13}})");
  REQUIRE(session.state() == SessionState::Error);
  REQUIRE(guard.invalidations == 1);
}

TEST_CASE("hello without a bounded nonempty session id fails closed") {
  FakeTransport transport;
  FakeGuard guard;
  XiaozhiSession session(transport, guard);
  TransportConfig config{"wss://api.example/path", "token", 2,
                         "device", "client", "root-ca"};
  REQUIRE(session.open(config));
  session.onTransportConnected();
  session.onTransportText(
      R"({"type":"hello","transport":"websocket","audio_params":{"sample_rate":24000,"frame_duration":60}})");
  REQUIRE(session.state() == SessionState::Error);
  REQUIRE(guard.invalidations == 1);
}
