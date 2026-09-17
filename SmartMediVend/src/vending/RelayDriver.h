#pragma once

#include <cstdint>

#include "IGpio.h"

namespace smv::vending {

enum class RelayState : uint8_t {
  SafeHigh = 0,
  Settling,
  ActiveLow,
  GuardGap,
  Fault
};

class RelayDriver {
 public:
  explicit RelayDriver(IGpio& gpio) : gpio_(gpio) {}

  void begin();
  bool startPulse(uint8_t channel, uint32_t nowMs);
  void process(uint32_t nowMs);
  void allOff();

  int takeCompletedChannel();
  bool isIdle() const { return state_ == RelayState::SafeHigh; }
  bool isHealthy() const { return state_ != RelayState::Fault; }
  RelayState state() const { return state_; }

 private:
  void selectChannel(uint8_t channel);

  IGpio& gpio_;
  RelayState state_ = RelayState::SafeHigh;
  uint8_t activeChannel_ = 0;
  int completedChannel_ = -1;
  uint32_t stateSinceMs_ = 0;
};

}  // namespace smv::vending
