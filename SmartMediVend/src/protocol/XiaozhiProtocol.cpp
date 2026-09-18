#include "XiaozhiProtocol.h"

#include "../core/JsonLite.h"

namespace smv::protocol {
namespace {

uint16_t readBe16(const uint8_t* data) {
  return static_cast<uint16_t>((static_cast<uint16_t>(data[0]) << 8U) |
                               static_cast<uint16_t>(data[1]));
}

uint32_t readBe32(const uint8_t* data) {
  return (static_cast<uint32_t>(data[0]) << 24U) |
         (static_cast<uint32_t>(data[1]) << 16U) |
         (static_cast<uint32_t>(data[2]) << 8U) |
         static_cast<uint32_t>(data[3]);
}

}  // namespace

AudioPacketView XiaozhiProtocol::parseBinary(uint8_t protocolVersion,
                                             const uint8_t* data,
                                             std::size_t length) {
  if (data == nullptr) return {FrameError::TooShort};
  if (protocolVersion == 1) {
    if (length == 0) return {FrameError::TooShort};
    return {FrameError::None, 0, 0, data, length};
  }
  if (protocolVersion == 2) {
    constexpr std::size_t headerBytes = 16;
    if (length < headerBytes) return {FrameError::TooShort};
    if (readBe16(data) != 2) return {FrameError::InvalidHeader};
    const uint32_t payloadSize = readBe32(data + 12);
    if (payloadSize != length - headerBytes) {
      return {FrameError::PayloadLengthMismatch};
    }
    return {FrameError::None, static_cast<uint8_t>(readBe16(data + 2)),
            readBe32(data + 8), data + headerBytes, payloadSize};
  }
  if (protocolVersion == 3) {
    constexpr std::size_t headerBytes = 4;
    if (length < headerBytes) return {FrameError::TooShort};
    const uint16_t payloadSize = readBe16(data + 2);
    if (payloadSize != length - headerBytes) {
      return {FrameError::PayloadLengthMismatch};
    }
    return {FrameError::None, data[0], 0, data + headerBytes, payloadSize};
  }
  return {FrameError::UnsupportedVersion};
}

TextMessageView XiaozhiProtocol::parseText(std::string_view json) {
  if (json.size() > kMaxTextBytes || !jsonlite::isValidObject(json)) {
    return {TextMessageType::Malformed, {}, {}};
  }
  jsonlite::ValueView type;
  if (!jsonlite::findMember(json, "type", type) ||
      type.kind != jsonlite::ValueKind::String) {
    return {TextMessageType::Malformed, {}, {}};
  }
  const auto name = type.stringValue();
  jsonlite::ValueView text;
  const std::string_view textValue =
      jsonlite::findMember(json, "text", text) ? text.stringValue()
                                                : std::string_view{};
  jsonlite::ValueView sessionId;
  const std::string_view sessionValue =
      jsonlite::findMember(json, "session_id", sessionId) &&
              sessionId.kind == jsonlite::ValueKind::String
          ? sessionId.stringValue()
          : std::string_view{};
  if (name == "hello") {
    return {TextMessageType::Hello, textValue, sessionValue};
  }
  if (name == "stt") return {TextMessageType::Stt, textValue, sessionValue};
  if (name == "llm") return {TextMessageType::Llm, textValue, sessionValue};
  if (name == "mcp") return {TextMessageType::Mcp, textValue, sessionValue};
  if (name == "system") {
    return {TextMessageType::System, textValue, sessionValue};
  }
  if (name == "alert") {
    return {TextMessageType::Alert, textValue, sessionValue};
  }
  if (name == "tts") {
    jsonlite::ValueView state;
    if (!jsonlite::findMember(json, "state", state)) {
      return {TextMessageType::Malformed, {}, {}};
    }
    const auto stateName = state.stringValue();
    if (stateName == "start") {
      return {TextMessageType::TtsStart, textValue, sessionValue};
    }
    if (stateName == "stop") {
      return {TextMessageType::TtsStop, textValue, sessionValue};
    }
    if (stateName == "sentence_start") {
      return {TextMessageType::TtsSentenceStart, textValue, sessionValue};
    }
  }
  return {TextMessageType::Unknown, textValue, sessionValue};
}

bool XiaozhiProtocol::isSessionScoped(TextMessageType type) {
  return type == TextMessageType::Mcp || type == TextMessageType::Stt ||
         type == TextMessageType::TtsStart ||
         type == TextMessageType::TtsStop ||
         type == TextMessageType::TtsSentenceStart;
}

bool XiaozhiProtocol::matchesActiveSession(
    const TextMessageView& message,
    std::string_view activeSessionId) {
  if (!isSessionScoped(message.type)) return true;
  return !activeSessionId.empty() && !message.sessionId.empty() &&
         message.sessionId == activeSessionId;
}

}  // namespace smv::protocol
