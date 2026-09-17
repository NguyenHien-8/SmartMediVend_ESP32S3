#include "ButtonController.h"

#include "../core/Elapsed.h"

namespace smv {

ButtonEvent ButtonController::process(bool rawPressed, uint32_t nowMs) {
  if (rawPressed != rawPressed_) {
    rawPressed_ = rawPressed;
    rawChangedAtMs_ = nowMs;
  }

  if (rawPressed_ != stablePressed_ &&
      elapsedMs(nowMs, rawChangedAtMs_, debounceMs_)) {
    stablePressed_ = rawPressed_;
    if (stablePressed_) {
      pressedAtMs_ = nowMs;
      longPressEmitted_ = false;
    } else {
      const bool emitShort = !longPressEmitted_;
      pressedAtMs_ = 0;
      longPressEmitted_ = false;
      if (emitShort) return ButtonEvent::ShortPress;
    }
  }

  if (stablePressed_ && !longPressEmitted_ &&
      elapsedMs(nowMs, pressedAtMs_, holdMs_)) {
    longPressEmitted_ = true;
    return ButtonEvent::LongPress;
  }
  return ButtonEvent::None;
}

uint8_t ButtonController::holdPercent(uint32_t nowMs) const {
  if (!stablePressed_) return 0;
  if (longPressEmitted_) return 100;
  const uint32_t held = static_cast<uint32_t>(nowMs - pressedAtMs_);
  if (held >= holdMs_) return 100;
  return static_cast<uint8_t>((held * 100U) / holdMs_);
}

}  // namespace smv
