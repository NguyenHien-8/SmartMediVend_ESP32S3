#pragma once

#include <Arduino.h>
#include "src/network/XiaozhiCertificates.h"

namespace smv {
namespace config {

#ifndef SMV_XIAOZHI_ROOT_CA_PEM
#define SMV_XIAOZHI_ROOT_CA_PEM smv::network::kXiaozhiRootCaBundle
#endif

#ifndef SMV_PRODUCTION_VENDING_ENABLED
#define SMV_PRODUCTION_VENDING_ENABLED 0
#endif

#ifndef SMV_PHARMACIST_APPROVED
#define SMV_PHARMACIST_APPROVED 0
#endif

#ifndef SMV_REVIEWED_CATALOG_VERSION
#define SMV_REVIEWED_CATALOG_VERSION ""
#endif

#ifndef SMV_REVIEWED_RULES_VERSION
#define SMV_REVIEWED_RULES_VERSION ""
#endif

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
static constexpr uint8_t TFT_ROTATION = 0;  // Portrait 240x320
static constexpr uint32_t TFT_SPI_FREQUENCY_HZ = 40000000UL;
static constexpr bool TFT_BACKLIGHT_ACTIVE_HIGH = true;
static constexpr uint32_t TFT_ANIMATION_INTERVAL_MS = 110UL;

// ---------- Voice confirmation ----------
static constexpr uint32_t CONFIRMATION_TIMEOUT_MS = 15000UL;

// ---------- Vending ----------
static constexpr bool PRODUCTION_VENDING_ENABLED =
    SMV_PRODUCTION_VENDING_ENABLED != 0;
static constexpr bool PHARMACIST_APPROVED = SMV_PHARMACIST_APPROVED != 0;
static constexpr const char* REVIEWED_CATALOG_VERSION =
    SMV_REVIEWED_CATALOG_VERSION;
static constexpr const char* REVIEWED_RULES_VERSION =
    SMV_REVIEWED_RULES_VERSION;
static constexpr const char* XIAOZHI_BOOTSTRAP_URL =
    "https://api.tenclass.net/xiaozhi/ota/";
static constexpr const char* XIAOZHI_ROOT_CA_PEM =
    SMV_XIAOZHI_ROOT_CA_PEM;
static constexpr uint32_t XIAOZHI_HTTP_TIMEOUT_MS = 10000UL;
static constexpr bool RELAY_ACTIVE_LOW = true;
static constexpr uint32_t RELAY_SETTLE_MS = 10UL;
static constexpr uint32_t RELAY_PULSE_MS = 500UL;
static constexpr uint32_t RELAY_GUARD_GAP_MS = 100UL;
static constexpr uint8_t MAX_MEDICINES_PER_TRANSACTION = 3;

// ---------- Bounded protocol input ----------
static constexpr size_t MAX_PROTOCOL_JSON_BYTES = 8192U;
static constexpr size_t MAX_MCP_ARGUMENT_BYTES = 4096U;

}  // namespace config
}  // namespace smv
