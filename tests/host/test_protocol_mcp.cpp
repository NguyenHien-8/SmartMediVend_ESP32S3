#include "TestHarness.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "src/mcp/McpServer.h"
#include "src/mcp/SmartMediVendTools.h"
#include "src/protocol/XiaozhiProtocol.h"

using smv::mcp::McpServer;
using smv::mcp::SmartMediVendTools;
using smv::protocol::FrameError;
using smv::protocol::TextMessageType;
using smv::protocol::XiaozhiProtocol;

namespace {

class ToolDataSource final : public smv::mcp::IToolDataSource {
 public:
  std::string deviceStatusJson() const override { return R"({"state":"idle"})"; }
  std::string inventoryJson() const override { return R"({"stock":80})"; }
  std::string medicineInfoJson(std::string_view) const override {
    return R"({"found":true})";
  }
  std::string submitSymptomDataJson(std::string_view) override {
    return R"({"accepted":true})";
  }
  std::string candidateStatusJson() const override {
    return R"({"status":"none"})";
  }
};

std::vector<uint8_t> v2Frame(uint32_t claimedSize,
                             std::initializer_list<uint8_t> payload) {
  std::vector<uint8_t> frame(16, 0);
  frame[1] = 2;
  frame[3] = 0;
  frame[8] = 0x00;
  frame[9] = 0x00;
  frame[10] = 0x00;
  frame[11] = 0x2A;
  frame[12] = static_cast<uint8_t>((claimedSize >> 24U) & 0xFFU);
  frame[13] = static_cast<uint8_t>((claimedSize >> 16U) & 0xFFU);
  frame[14] = static_cast<uint8_t>((claimedSize >> 8U) & 0xFFU);
  frame[15] = static_cast<uint8_t>(claimedSize & 0xFFU);
  frame.insert(frame.end(), payload.begin(), payload.end());
  return frame;
}

}  // namespace

TEST_CASE("Xiaozhi v2 rejects short header and payload overrun") {
  const std::array<uint8_t, 2> shortFrame{{0, 2}};
  REQUIRE(XiaozhiProtocol::parseBinary(2, shortFrame.data(), shortFrame.size())
              .error == FrameError::TooShort);

  const auto overrun = v2Frame(200, {1, 2, 3});
  REQUIRE(XiaozhiProtocol::parseBinary(2, overrun.data(), overrun.size()).error ==
          FrameError::PayloadLengthMismatch);
}

TEST_CASE("Xiaozhi v2 and v3 expose only validated payload") {
  const auto v2 = v2Frame(3, {0x11, 0x22, 0x33});
  const auto parsedV2 = XiaozhiProtocol::parseBinary(2, v2.data(), v2.size());
  REQUIRE(parsedV2.ok());
  REQUIRE(parsedV2.timestamp == 42);
  REQUIRE(parsedV2.payloadSize == 3);
  REQUIRE(parsedV2.payload[2] == 0x33);

  const std::array<uint8_t, 6> v3{{0, 0, 0, 2, 0xAA, 0xBB}};
  const auto parsedV3 = XiaozhiProtocol::parseBinary(3, v3.data(), v3.size());
  REQUIRE(parsedV3.ok());
  REQUIRE(parsedV3.payloadSize == 2);
  REQUIRE(parsedV3.payload[0] == 0xAA);
}

TEST_CASE("Xiaozhi text parser recognizes supported types and rejects malformed JSON") {
  REQUIRE(XiaozhiProtocol::parseText(R"({"type":"stt","text":"xin chao"})")
              .type == TextMessageType::Stt);
  REQUIRE(XiaozhiProtocol::parseText(R"({"type":"tts","state":"start"})")
              .type == TextMessageType::TtsStart);
  REQUIRE(XiaozhiProtocol::parseText("not-json").type ==
          TextMessageType::Malformed);
}

TEST_CASE("MCP tool list contains no vend, relay, or raw GPIO authority") {
  const auto names = SmartMediVendTools::names();
  REQUIRE(names.size() == 5);
  REQUIRE(names[0] == "smartmedivend.get_device_status");
  REQUIRE(names[1] == "smartmedivend.get_inventory");
  REQUIRE(names[2] == "smartmedivend.get_medicine_info");
  REQUIRE(names[3] == "smartmedivend.submit_symptom_data");
  REQUIRE(names[4] == "smartmedivend.get_candidate_status");
  for (const auto name : names) {
    REQUIRE(name != "smartmedivend.vend");
    REQUIRE(name != "smartmedivend.relay_on");
    REQUIRE(name != "smartmedivend.relay_off");
    REQUIRE(name != "smartmedivend.raw_gpio_write");
  }
}

TEST_CASE("MCP handles initialize and rejects unknown or unsafe calls") {
  ToolDataSource data;
  SmartMediVendTools tools(data);
  McpServer server(tools);

  const auto initialized = server.handle(
      R"({"jsonrpc":"2.0","id":1,"method":"initialize","params":{}})");
  REQUIRE(initialized.find("2024-11-05") != std::string::npos);

  const auto unknown = server.handle(
      R"({"jsonrpc":"2.0","id":2,"method":"relay_on","params":{}})");
  REQUIRE(unknown.find("-32601") != std::string::npos);

  const auto unsafeSubmission = server.handle(
      R"({"jsonrpc":"2.0","id":3,"method":"tools/call","params":{"name":"smartmedivend.submit_symptom_data","arguments":{"session_id":"s","vend":true}}})");
  REQUIRE(unsafeSubmission.find("FORBIDDEN_AUTHORITY_FIELD") !=
          std::string::npos);
}

TEST_CASE("MCP rejects oversized, malformed, and missing JSON-RPC fields") {
  ToolDataSource data;
  SmartMediVendTools tools(data);
  McpServer server(tools);
  REQUIRE(server.handle("not-json").find("-32700") != std::string::npos);
  REQUIRE(server.handle(R"({"id":1,"method":"initialize"})")
              .find("-32600") != std::string::npos);
  REQUIRE(server.handle(std::string(8193, 'x')).find("REQUEST_TOO_LARGE") !=
          std::string::npos);
}
