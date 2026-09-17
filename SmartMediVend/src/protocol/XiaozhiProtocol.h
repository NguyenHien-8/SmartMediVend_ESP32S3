#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "XiaozhiMessage.h"

namespace smv::protocol {

class XiaozhiProtocol {
 public:
  static constexpr std::size_t kMaxTextBytes = 8192;

  static AudioPacketView parseBinary(uint8_t protocolVersion,
                                     const uint8_t* data,
                                     std::size_t length);
  static TextMessageView parseText(std::string_view json);
};

}  // namespace smv::protocol
