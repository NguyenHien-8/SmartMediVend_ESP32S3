#pragma once

#include <cstdint>

namespace smv {

constexpr bool elapsedMs(uint32_t now, uint32_t since, uint32_t interval) {
  return static_cast<uint32_t>(now - since) >= interval;
}

}  // namespace smv
