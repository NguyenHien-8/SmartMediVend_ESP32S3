#pragma once

#include <cstdint>

namespace smv {

enum class ButtonEvent : uint8_t { None = 0, ShortPress, LongPress };

class ButtonController {
 public:
  ButtonController(uint32_t debounceMs, uint32_t holdMs)
      : debounceMs_(debounceMs), holdMs_(holdMs) {}

  ButtonEvent process(bool rawPressed, uint32_t nowMs);
  bool isPressed() const { return stablePressed_; }
  uint8_t holdPercent(uint32_t nowMs) const;

 private:
  uint32_t debounceMs_;
  uint32_t holdMs_;
  bool rawPressed_ = false;
  bool stablePressed_ = false;
  bool longPressEmitted_ = false;
  uint32_t rawChangedAtMs_ = 0;
  uint32_t pressedAtMs_ = 0;
};

}  // namespace smv
