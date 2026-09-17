#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "HardwarePins.h"
#include "src/vending/IGpio.h"

namespace smv::test {

class FakeGpio final : public vending::IGpio {
 public:
  void configureOutput(uint8_t pin, bool initialHigh) override {
    levels_[pin] = initialHigh;
  }

  void write(uint8_t pin, bool high) override {
    levels_[pin] = high;
    if (pin == pins::MUX_SIG && !high) {
      const uint8_t channel =
          static_cast<uint8_t>((levels_[pins::MUX_S0] ? 1U : 0U) |
                               (levels_[pins::MUX_S1] ? 2U : 0U) |
                               (levels_[pins::MUX_S2] ? 4U : 0U) |
                               (levels_[pins::MUX_S3] ? 8U : 0U));
      pulseOrder.push_back(channel);
    }
  }

  bool level(uint8_t pin) const { return levels_[pin]; }

  std::vector<uint8_t> pulseOrder;

 private:
  std::array<bool, 49> levels_{};
};

}  // namespace smv::test
