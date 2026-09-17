#include "RelayDriver.h"

#include "../../AppConfig.h"
#include "../../HardwarePins.h"
#include "../core/Elapsed.h"

namespace smv::vending {

void RelayDriver::begin() {
  gpio_.configureOutput(pins::MUX_SIG, true);
  gpio_.configureOutput(pins::MUX_S0, false);
  gpio_.configureOutput(pins::MUX_S1, false);
  gpio_.configureOutput(pins::MUX_S2, false);
  gpio_.configureOutput(pins::MUX_S3, false);
  allOff();
}

bool RelayDriver::startPulse(uint8_t channel, uint32_t nowMs) {
  if (!isIdle() || channel > 15) {
    return false;
  }
  gpio_.write(pins::MUX_SIG, true);
  selectChannel(channel);
  activeChannel_ = channel;
  completedChannel_ = -1;
  stateSinceMs_ = nowMs;
  state_ = RelayState::Settling;
  return true;
}

void RelayDriver::process(uint32_t nowMs) {
  switch (state_) {
    case RelayState::Settling:
      if (elapsedMs(nowMs, stateSinceMs_, config::RELAY_SETTLE_MS)) {
        gpio_.write(pins::MUX_SIG, false);
        stateSinceMs_ = nowMs;
        state_ = RelayState::ActiveLow;
      }
      break;
    case RelayState::ActiveLow:
      if (elapsedMs(nowMs, stateSinceMs_, config::RELAY_PULSE_MS)) {
        gpio_.write(pins::MUX_SIG, true);
        completedChannel_ = activeChannel_;
        stateSinceMs_ = nowMs;
        state_ = RelayState::GuardGap;
      }
      break;
    case RelayState::GuardGap:
      if (elapsedMs(nowMs, stateSinceMs_, config::RELAY_GUARD_GAP_MS)) {
        state_ = RelayState::SafeHigh;
      }
      break;
    case RelayState::SafeHigh:
    case RelayState::Fault:
    default:
      break;
  }
}

void RelayDriver::allOff() {
  gpio_.write(pins::MUX_SIG, true);
  completedChannel_ = -1;
  state_ = RelayState::SafeHigh;
}

int RelayDriver::takeCompletedChannel() {
  const int result = completedChannel_;
  completedChannel_ = -1;
  return result;
}

void RelayDriver::selectChannel(uint8_t channel) {
  gpio_.write(pins::MUX_S0, (channel & 0x01U) != 0U);
  gpio_.write(pins::MUX_S1, (channel & 0x02U) != 0U);
  gpio_.write(pins::MUX_S2, (channel & 0x04U) != 0U);
  gpio_.write(pins::MUX_S3, (channel & 0x08U) != 0U);
}

}  // namespace smv::vending
