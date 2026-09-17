#pragma once

#include <cstdint>

namespace smv::vending {

class IGpio {
 public:
  virtual ~IGpio() = default;
  virtual void configureOutput(uint8_t pin, bool initialHigh) = 0;
  virtual void write(uint8_t pin, bool high) = 0;
};

}  // namespace smv::vending
