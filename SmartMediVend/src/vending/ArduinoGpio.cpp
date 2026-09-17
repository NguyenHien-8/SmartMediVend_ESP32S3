#include "ArduinoGpio.h"

#include <Arduino.h>

namespace smv::vending {

void ArduinoGpio::configureOutput(uint8_t pin, bool initialHigh) {
  // Preload the safe latch level before switching the pin to output mode.
  digitalWrite(pin, initialHigh ? HIGH : LOW);
  pinMode(pin, OUTPUT);
}

void ArduinoGpio::write(uint8_t pin, bool high) {
  digitalWrite(pin, high ? HIGH : LOW);
}

}  // namespace smv::vending
