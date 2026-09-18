#pragma once

#include <cstddef>
#include <Arduino.h>

namespace smv {
namespace pins {

// TFT 2.4" SPI (ST7789, write-only SPI)
static constexpr uint8_t TFT_CS   = 10;
static constexpr uint8_t TFT_RST  = 14;
static constexpr uint8_t TFT_DC   = 9;
static constexpr uint8_t TFT_MOSI = 11;
static constexpr uint8_t TFT_SCLK = 12;
static constexpr uint8_t TFT_BL   = 13;

// SET WIFI button:
// - Active HIGH
// - External 4.7 kOhm pull-down resistor
static constexpr uint8_t BT_SETWIFI = 18;

// INMP441 microphone (I2S RX)
static constexpr uint8_t INMP441_SD  = 6;
static constexpr uint8_t INMP441_WS  = 4;
static constexpr uint8_t INMP441_SCK = 5;

// MAX98357A amplifier (I2S TX)
static constexpr uint8_t MAX98357_LRC  = 16;
static constexpr uint8_t MAX98357_BCLK = 15;
static constexpr uint8_t MAX98357_DIN  = 7;

// CD74HC4067 relay selector. GPIO35-37 are unavailable on N16R8 because
// they are used by the module's Octal PSRAM.
static constexpr uint8_t MUX_S0  = 39;
static constexpr uint8_t MUX_S1  = 40;
static constexpr uint8_t MUX_S2  = 41;
static constexpr uint8_t MUX_S3  = 42;
static constexpr uint8_t MUX_SIG = 17;

// GPIO43 and GPIO44 are intentionally unused. Earlier IR/radar sensor
// revisions were removed from the approved hardware design.
static constexpr uint8_t UNUSED_GPIO43 = 43;
static constexpr uint8_t UNUSED_GPIO44 = 44;

constexpr uint8_t ASSIGNED_PINS[] = {
    TFT_CS,       TFT_RST,      TFT_DC,       TFT_MOSI,      TFT_SCLK,
    TFT_BL,       BT_SETWIFI,   INMP441_SD,   INMP441_WS,    INMP441_SCK,
    MAX98357_LRC, MAX98357_BCLK, MAX98357_DIN, MUX_S0,       MUX_S1,
    MUX_S2,       MUX_S3,       MUX_SIG};

template <std::size_t N>
constexpr bool pinsAreUnique(const uint8_t (&values)[N]) {
  for (std::size_t i = 0; i < N; ++i) {
    for (std::size_t j = i + 1; j < N; ++j) {
      if (values[i] == values[j]) {
        return false;
      }
    }
  }
  return true;
}

static_assert(pinsAreUnique(ASSIGNED_PINS),
              "SmartMediVend GPIO conflict: assigned pins must be unique.");
static_assert(MUX_SIG != 35 && MUX_SIG != 36 && MUX_SIG != 37,
              "ESP32-S3-N16R8 GPIO35-37 are reserved by Octal PSRAM.");

}  // namespace pins
}  // namespace smv
