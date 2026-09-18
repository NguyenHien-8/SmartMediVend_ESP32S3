#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace smv::protocol {

enum class FrameError : uint8_t {
  None = 0,
  TooShort,
  UnsupportedVersion,
  InvalidHeader,
  PayloadLengthMismatch
};

struct AudioPacketView {
  FrameError error = FrameError::None;
  uint8_t type = 0;
  uint32_t timestamp = 0;
  const uint8_t* payload = nullptr;
  std::size_t payloadSize = 0;
  bool ok() const { return error == FrameError::None; }
};

enum class TextMessageType : uint8_t {
  Malformed = 0,
  Unknown,
  Hello,
  Stt,
  TtsStart,
  TtsStop,
  TtsSentenceStart,
  Llm,
  Mcp,
  System,
  Alert
};

struct TextMessageView {
  TextMessageType type = TextMessageType::Malformed;
  std::string_view text;
  std::string_view sessionId;
};

}  // namespace smv::protocol
