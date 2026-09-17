#pragma once

#include <Arduino.h>

namespace smv {
namespace config {

// ---------- Wi-Fi ----------
static const char WIFI_HOSTNAME[] = "TINIHI";
static const char WIFI_PORTAL_SSID[] = "TINIHI-Setup";

// Development/setup password.
// Change this before deploying a public machine.
static const char WIFI_PORTAL_PASSWORD[] = "78787878";

static constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 12000UL;
static constexpr uint32_t WIFI_PORTAL_TIMEOUT_MS  = 300000UL;  // 5 minutes
static constexpr uint32_t WIFI_BUTTON_HOLD_MS     = 2000UL;
static constexpr uint32_t WIFI_BUTTON_DEBOUNCE_MS = 35UL;
static constexpr uint32_t WIFI_TELEMETRY_MS       = 5000UL;
static constexpr bool WIFI_LOGGING_ENABLED        = true;

// ---------- TFT ----------
static constexpr uint8_t TFT_ROTATION = 1;  // Landscape 320x240
static constexpr uint32_t TFT_SPI_FREQUENCY_HZ = 40000000UL;
static constexpr bool TFT_BACKLIGHT_ACTIVE_HIGH = true;
static constexpr uint32_t TFT_ANIMATION_INTERVAL_MS = 110UL;

}  // namespace config
}  // namespace smv
