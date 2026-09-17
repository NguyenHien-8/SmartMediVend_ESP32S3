#pragma once

#include "IGpio.h"

namespace smv::vending {

class ArduinoGpio final : public IGpio {
 public:
  void configureOutput(uint8_t pin, bool initialHigh) override;
  void write(uint8_t pin, bool high) override;
};

}  // namespace smv::vending
