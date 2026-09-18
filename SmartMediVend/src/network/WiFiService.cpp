#include "WiFiService.h"

#include "../../AppConfig.h"
#include "../../HardwarePins.h"
#include "../core/Elapsed.h"

namespace smv {

namespace {
constexpr uint32_t ERROR_VISIBLE_MS = 5000UL;
}

WiFiService::WiFiService()
    : button_(config::WIFI_BUTTON_DEBOUNCE_MS,
              config::WIFI_BUTTON_HOLD_MS) {}

bool WiFiService::begin() {
  // Keep the active-HIGH input deterministic even if the external 4.7 kOhm
  // pull-down is temporarily disconnected during bench testing.
  pinMode(pins::BT_SETWIFI, INPUT_PULLDOWN);

  const uint32_t now = millis();
  button_.process(digitalRead(pins::BT_SETWIFI) == HIGH, now);

  portal_.setLogging(config::WIFI_LOGGING_ENABLED);
  portal_.setConnectTimeout(config::WIFI_CONNECT_TIMEOUT_MS);
  portal_.setAutoReconnect(true);

  if (!portal_.setHostname(config::WIFI_HOSTNAME) &&
      config::WIFI_LOGGING_ENABLED) {
    Serial.print(F("[SMV] Wi-Fi hostname warning: "));
    Serial.println(portal_.lastError());
  }

  portal_.onPortalStarted([this]() { portalStartError_ = false; });
  portal_.onConnected([this]() { portalStartError_ = false; });

  setUiState(UiState::Connecting);
  const bool connected =
      portal_.connectSaved(config::WIFI_CONNECT_TIMEOUT_MS);
  syncUiState(millis(), true);
  return connected;
}

void WiFiService::process() {
  portal_.process();

  const uint32_t now = millis();
  const ButtonEvent event = button_.process(
      digitalRead(pins::BT_SETWIFI) == HIGH, now);
  if (event != ButtonEvent::None) {
    pendingButtonEvent_ = event;
  }
  if (event == ButtonEvent::LongPress && !portal_.isPortalActive()) {
    if (config::WIFI_LOGGING_ENABLED) {
      Serial.println(F("[SMV] SET WIFI held for 2 s -> open portal"));
    }
    requestConfigPortal();
  }

  syncUiState(now, false);
}

bool WiFiService::requestConfigPortal() {
  if (portal_.isPortalActive()) return true;

  portalStartError_ = false;
  const bool started = portal_.startConfigPortalAsync(
      config::WIFI_PORTAL_SSID,
      config::WIFI_PORTAL_PASSWORD,
      config::WIFI_PORTAL_TIMEOUT_MS);

  if (!started) {
    portalStartError_ = true;
    portalStartErrorAt_ = millis();
    lastError_ = portal_.lastError();
    setUiState(UiState::Error);
    if (config::WIFI_LOGGING_ENABLED) {
      Serial.print(F("[SMV] Unable to open Wi-Fi portal: "));
      Serial.println(lastError_);
    }
    return false;
  }

  refreshPortalTelemetry();
  setUiState(UiState::Portal);
  return true;
}

ButtonEvent WiFiService::takeButtonEvent() {
  const ButtonEvent event = pendingButtonEvent_;
  pendingButtonEvent_ = ButtonEvent::None;
  return event;
}

uint8_t WiFiService::configButtonHoldPercent() const {
  return button_.holdPercent(millis());
}

void WiFiService::syncUiState(uint32_t now, bool forceRefresh) {
  if (portalStartError_) {
    if (!elapsedMs(now, portalStartErrorAt_, ERROR_VISIBLE_MS)) {
      setUiState(UiState::Error);
      return;
    }
    portalStartError_ = false;
  }

  if (portal_.isPortalActive()) {
    refreshPortalTelemetry();
    setUiState(portal_.isPortalConnectionAttemptActive()
                   ? UiState::PortalConnecting
                   : UiState::Portal);
    return;
  }

  if (portal_.isConnected()) {
    refreshConnectedTelemetry(now, forceRefresh);
    setUiState(UiState::Connected);
    return;
  }

  switch (portal_.state()) {
    case ESP32WiFiPortal::State::Connecting:
      setUiState(UiState::Connecting);
      break;
    case ESP32WiFiPortal::State::Connected:
      refreshConnectedTelemetry(now, forceRefresh);
      setUiState(UiState::Connected);
      break;
    case ESP32WiFiPortal::State::Portal:
      refreshPortalTelemetry();
      setUiState(portal_.isPortalConnectionAttemptActive()
                     ? UiState::PortalConnecting
                     : UiState::Portal);
      break;
    case ESP32WiFiPortal::State::Failed:
      lastError_ = portal_.lastError();
      setUiState(UiState::Offline);
      break;
    case ESP32WiFiPortal::State::Idle:
    default:
      setUiState(UiState::Offline);
      break;
  }
}

void WiFiService::refreshConnectedTelemetry(uint32_t now,
                                            bool forceRefresh) {
  if (!forceRefresh &&
      !elapsedMs(now, lastTelemetryAt_, config::WIFI_TELEMETRY_MS)) {
    return;
  }
  lastTelemetryAt_ = now;
  ssid_ = WiFi.SSID();
  localIP_ = WiFi.localIP();
  rssi_ = WiFi.RSSI();
}

void WiFiService::refreshPortalTelemetry() {
  portalSSID_ = portal_.portalSSID();
  portalIP_ = portal_.portalIP();
}

void WiFiService::setUiState(UiState state) {
  if (uiState_ == state) return;
  uiState_ = state;
  if (config::WIFI_LOGGING_ENABLED) {
    Serial.print(F("[SMV] Wi-Fi UI state -> "));
    Serial.println(static_cast<unsigned int>(uiState_));
  }
}

}  // namespace smv
