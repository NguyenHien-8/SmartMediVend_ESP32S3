#pragma once

#include <cstdint>

namespace smv {

enum class AppEventType : uint8_t {
  None = 0,
  WifiConnected,
  WifiDisconnected,
  WifiPortalStarted,
  CloudConnecting,
  ActivationRequired,
  CloudConnected,
  CloudDisconnected,
  StartListening,
  StopListening,
  SttReceived,
  TtsStarted,
  TtsStopped,
  CandidateReady,
  NeedMoreInformation,
  SafetyRejected,
  UserConfirmed,
  UserCancelled,
  VendStarted,
  VendCompleted,
  VendFailed,
  Timeout
};

struct AppEvent {
  AppEventType type = AppEventType::None;
  uint32_t timestampMs = 0;
};

}  // namespace smv
