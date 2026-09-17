#pragma once

#include <Arduino.h>

namespace smv {
namespace pins {

// TFT 2.4" SPI (ILI9341, write-only SPI)
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

// Catch accidental GPIO reuse at compile time.
static_assert(
    TFT_CS   != TFT_RST  &&
    TFT_CS   != TFT_DC   &&
    TFT_CS   != TFT_MOSI &&
    TFT_CS   != TFT_SCLK &&
    TFT_CS   != TFT_BL   &&
    TFT_CS   != BT_SETWIFI &&
    TFT_RST  != TFT_DC   &&
    TFT_RST  != TFT_MOSI &&
    TFT_RST  != TFT_SCLK &&
    TFT_RST  != TFT_BL   &&
    TFT_RST  != BT_SETWIFI &&
    TFT_DC   != TFT_MOSI &&
    TFT_DC   != TFT_SCLK &&
    TFT_DC   != TFT_BL   &&
    TFT_DC   != BT_SETWIFI &&
    TFT_MOSI != TFT_SCLK &&
    TFT_MOSI != TFT_BL   &&
    TFT_MOSI != BT_SETWIFI &&
    TFT_SCLK != TFT_BL   &&
    TFT_SCLK != BT_SETWIFI &&
    TFT_BL   != BT_SETWIFI,
    "SmartMediVend GPIO conflict: each assigned pin must be unique.");

}  // namespace pins
}  // namespace smv
