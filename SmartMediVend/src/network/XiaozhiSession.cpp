#include "XiaozhiSession.h"

#include <charconv>

#include "../core/JsonLite.h"

namespace smv::network {
namespace {

bool parseUnsigned(const jsonlite::ValueView& value, uint32_t& result) {
  if (value.kind != jsonlite::ValueKind::Primitive || value.raw.empty()) {
    return false;
  }
  const char* begin = value.raw.data();
  const auto parsed =
      std::from_chars(begin, begin + value.raw.size(), result);
  return parsed.ec == std::errc{} &&
         parsed.ptr == begin + value.raw.size();
}

bool validSampleRate(uint32_t rate) {
  return rate == 16000U || rate == 24000U || rate == 48000U;
}

bool validFrameDuration(uint32_t duration) {
  return duration == 20U || duration == 40U || duration == 60U;
}

}  // namespace

XiaozhiSession::XiaozhiSession(IXiaozhiTransport& transport,
                               ICloudSessionGuard& guard)
    : transport_(transport), guard_(guard) {
  transport_.setListener(this);
}

bool XiaozhiSession::open(const TransportConfig& config) {
  if (config.url.rfind("wss://", 0) != 0 || config.rootCaPem.empty() ||
      config.protocolVersion < 1 || config.protocolVersion > 3 ||
      config.deviceId.empty() || config.clientId.empty()) {
    state_ = SessionState::Error;
    return false;
  }
  config_ = config;
  contextInvalidated_ = false;
  sessionId_.clear();
  downlinkSampleRate_ = 16000;
  downlinkFrameDurationMs_ = 60;
  state_ = SessionState::Connecting;
  if (!transport_.connect(config_)) {
    failClosed();
    return false;
  }
  return true;
}

void XiaozhiSession::close() {
  transport_.disconnect();
  state_ = SessionState::Disconnected;
  invalidateOnce();
}

bool XiaozhiSession::sendText(std::string_view text) {
  return state_ == SessionState::Ready && transport_.sendText(text);
}

bool XiaozhiSession::sendAudio(const uint8_t* data, std::size_t size) {
  return state_ == SessionState::Ready && data != nullptr && size > 0 &&
         transport_.sendBinary(data, size);
}

uint32_t XiaozhiSession::reconnectDelayMs(uint8_t failures,
                                          uint16_t jitterMs) {
  constexpr uint32_t kBase = 1000U;
  constexpr uint32_t kCap = 60000U;
  const uint8_t shift = failures > 6U ? 6U : failures;
  uint32_t delay = kBase << shift;
  if (delay > kCap) delay = kCap;
  const uint32_t remaining = kCap - delay;
  return delay + (jitterMs < remaining ? jitterMs : remaining);
}

void XiaozhiSession::onTransportConnected() {
  if (state_ != SessionState::Connecting) return;
  state_ = SessionState::AwaitingHello;
  if (!transport_.sendText(helloMessage())) failClosed();
}

void XiaozhiSession::onTransportDisconnected() {
  state_ = SessionState::Disconnected;
  sessionId_.clear();
  invalidateOnce();
}

void XiaozhiSession::onTransportText(std::string_view text) {
  if (state_ != SessionState::AwaitingHello || text.size() > 8192U ||
      !jsonlite::isValidObject(text)) {
    if (state_ == SessionState::AwaitingHello) failClosed();
    return;
  }

  jsonlite::ValueView type;
  jsonlite::ValueView transport;
  jsonlite::ValueView audio;
  if (!jsonlite::findMember(text, "type", type) ||
      type.stringValue() != "hello" ||
      !jsonlite::findMember(text, "transport", transport) ||
      transport.stringValue() != "websocket" ||
      !jsonlite::findMember(text, "audio_params", audio) ||
      audio.kind != jsonlite::ValueKind::Object) {
    failClosed();
    return;
  }

  jsonlite::ValueView sampleRate;
  jsonlite::ValueView frameDuration;
  uint32_t parsedRate = 0;
  uint32_t parsedDuration = 0;
  if (!jsonlite::findMember(audio.raw, "sample_rate", sampleRate) ||
      !parseUnsigned(sampleRate, parsedRate) || !validSampleRate(parsedRate) ||
      !jsonlite::findMember(audio.raw, "frame_duration", frameDuration) ||
      !parseUnsigned(frameDuration, parsedDuration) ||
      !validFrameDuration(parsedDuration)) {
    failClosed();
    return;
  }

  jsonlite::ValueView sessionId;
  if (jsonlite::findMember(text, "session_id", sessionId) &&
      sessionId.kind == jsonlite::ValueKind::String &&
      sessionId.stringValue().size() <= 128U) {
    sessionId_.assign(sessionId.stringValue());
  }
  downlinkSampleRate_ = parsedRate;
  downlinkFrameDurationMs_ = static_cast<uint16_t>(parsedDuration);
  state_ = SessionState::Ready;
}

void XiaozhiSession::onTransportBinary(const uint8_t*, std::size_t) {
  // Audio payload ownership is attached by AudioService in the composition
  // layer; session lifecycle validation remains here.
}

void XiaozhiSession::onTransportError() { failClosed(); }

void XiaozhiSession::failClosed() {
  state_ = SessionState::Error;
  sessionId_.clear();
  invalidateOnce();
  transport_.disconnect();
}

void XiaozhiSession::invalidateOnce() {
  if (contextInvalidated_) return;
  contextInvalidated_ = true;
  guard_.invalidateCloudContext();
}

std::string XiaozhiSession::helloMessage() const {
  return std::string("{\"type\":\"hello\",\"version\":") +
         std::to_string(config_.protocolVersion) +
         ",\"features\":{\"mcp\":true},\"transport\":\"websocket\"," +
         "\"audio_params\":{\"format\":\"opus\",\"sample_rate\":16000," +
         "\"channels\":1,\"frame_duration\":60}}";
}

}  // namespace smv::network
