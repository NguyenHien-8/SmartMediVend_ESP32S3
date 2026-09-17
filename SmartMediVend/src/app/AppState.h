#pragma once

#include <cstdint>

namespace smv {

enum class AppState : uint8_t {
  Booting = 0,
  WifiConnecting,
  WifiPortal,
  Offline,
  CloudConnecting,
  Idle,
  Listening,
  Processing,
  Speaking,
  AwaitingConfirmation,
  Dispensing,
  Recovering,
  Error
};

}  // namespace smv
