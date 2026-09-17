#include "ManageWiFiConnections.h"

#include "AppConfig.h"
#include "HardwarePins.h"

namespace smv {

namespace {
static constexpr uint32_t ERROR_VISIBLE_MS = 5000UL;

bool elapsed(uint32_t now, uint32_t since, uint32_t interval) {
  return static_cast<uint32_t>(now - since) >= interval;
}
}  // namespace

bool ManageWiFiConnections::begin() {
  pinMode(pins::BT_SETWIFI, INPUT);  // External 4.7 kOhm pull-down is fitted.

  _buttonRaw = (digitalRead(pins::BT_SETWIFI) == HIGH);
  _buttonStable = _buttonRaw;
  _buttonRawChangedAt = millis();
  _buttonPressedAt = _buttonStable ? millis() : 0;

  _portal.setLogging(config::WIFI_LOGGING_ENABLED);
  _portal.setConnectTimeout(config::WIFI_CONNECT_TIMEOUT_MS);
  _portal.setAutoReconnect(true);

  if (!_portal.setHostname(config::WIFI_HOSTNAME) &&
      config::WIFI_LOGGING_ENABLED) {
    Serial.print(F("[SMV] Wi-Fi hostname warning: "));
    Serial.println(_portal.lastError());
  }

  // Keep callbacks light. The main loop remains the single place that renders
  // the UI and runs future application services.
  _portal.onPortalStarted([this]() {
    _portalStartError = false;
  });

  _portal.onConnected([this]() {
    _portalStartError = false;
  });

  setUiState(UiState::Connecting);

  const bool connected =
      _portal.connectSaved(config::WIFI_CONNECT_TIMEOUT_MS);

  syncUiState(millis(), true);
  return connected;
}

void ManageWiFiConnections::process() {
  // ESP32WiFiPortal v2.1.2 uses this cooperative call for:
  // DNS/WebServer portal service, pending credentials, reconnect and backoff.
  _portal.process();

  const uint32_t now = millis();
  processConfigButton(now);
  syncUiState(now, false);
}

bool ManageWiFiConnections::requestConfigPortal() {
  if (_portal.isPortalActive()) {
    return true;
  }

  _portalStartError = false;

  const bool started = _portal.startConfigPortalAsync(
      config::WIFI_PORTAL_SSID,
      config::WIFI_PORTAL_PASSWORD,
      config::WIFI_PORTAL_TIMEOUT_MS);

  if (!started) {
    _portalStartError = true;
    _portalStartErrorAt = millis();
    _lastError = _portal.lastError();
    setUiState(UiState::Error);

    if (config::WIFI_LOGGING_ENABLED) {
      Serial.print(F("[SMV] Unable to open Wi-Fi portal: "));
      Serial.println(_lastError);
    }
    return false;
  }

  refreshPortalTelemetry();
  setUiState(UiState::Portal);
  return true;
}

uint8_t ManageWiFiConnections::configButtonHoldPercent() const {
  if (!_buttonStable) {
    return 0;
  }

  if (_buttonHandled) {
    return 100;
  }

  const uint32_t heldMs =
      static_cast<uint32_t>(millis() - _buttonPressedAt);

  if (heldMs >= config::WIFI_BUTTON_HOLD_MS) {
    return 100;
  }

  return static_cast<uint8_t>(
      (heldMs * 100UL) / config::WIFI_BUTTON_HOLD_MS);
}

void ManageWiFiConnections::processConfigButton(uint32_t now) {
  const bool rawPressed = (digitalRead(pins::BT_SETWIFI) == HIGH);

  if (rawPressed != _buttonRaw) {
    _buttonRaw = rawPressed;
    _buttonRawChangedAt = now;
  }

  if (_buttonRaw != _buttonStable &&
      elapsed(now, _buttonRawChangedAt, config::WIFI_BUTTON_DEBOUNCE_MS)) {
    _buttonStable = _buttonRaw;

    if (_buttonStable) {
      _buttonPressedAt = now;
      _buttonHandled = false;
    } else {
      _buttonPressedAt = 0;
      _buttonHandled = false;
    }
  }

  if (_buttonStable &&
      !_buttonHandled &&
      elapsed(now, _buttonPressedAt, config::WIFI_BUTTON_HOLD_MS)) {
    // Fire once per physical press, even if the user keeps holding the button.
    _buttonHandled = true;

    if (!_portal.isPortalActive()) {
      if (config::WIFI_LOGGING_ENABLED) {
        Serial.println(F("[SMV] SET WIFI held for 2 s -> open portal"));
      }
      requestConfigPortal();
    }
  }
}

void ManageWiFiConnections::syncUiState(uint32_t now, bool forceRefresh) {
  if (_portalStartError) {
    if (!elapsed(now, _portalStartErrorAt, ERROR_VISIBLE_MS)) {
      setUiState(UiState::Error);
      return;
    }
    _portalStartError = false;
  }

  if (_portal.isPortalActive()) {
    refreshPortalTelemetry();

    if (_portal.isPortalConnectionAttemptActive()) {
      setUiState(UiState::PortalConnecting);
    } else {
      setUiState(UiState::Portal);
    }
    return;
  }

  if (_portal.isConnected()) {
    refreshConnectedTelemetry(now, forceRefresh);
    setUiState(UiState::Connected);
    return;
  }

  switch (_portal.state()) {
    case ESP32WiFiPortal::State::Connecting:
      setUiState(UiState::Connecting);
      break;

    case ESP32WiFiPortal::State::Connected:
      refreshConnectedTelemetry(now, forceRefresh);
      setUiState(UiState::Connected);
      break;

    case ESP32WiFiPortal::State::Portal:
      refreshPortalTelemetry();
      setUiState(_portal.isPortalConnectionAttemptActive()
                     ? UiState::PortalConnecting
                     : UiState::Portal);
      break;

    case ESP32WiFiPortal::State::Failed:
      _lastError = _portal.lastError();
      setUiState(UiState::Offline);
      break;

    case ESP32WiFiPortal::State::Idle:
    default:
      setUiState(UiState::Offline);
      break;
  }
}

void ManageWiFiConnections::refreshConnectedTelemetry(uint32_t now, bool forceRefresh) {
  if (!forceRefresh &&
      !elapsed(now, _lastTelemetryAt, config::WIFI_TELEMETRY_MS)) {
    return;
  }

  _lastTelemetryAt = now;
  _ssid = WiFi.SSID();
  _localIP = WiFi.localIP();
  _rssi = WiFi.RSSI();
}

void ManageWiFiConnections::refreshPortalTelemetry() {
  _portalSSID = _portal.portalSSID();
  _portalIP = _portal.portalIP();
}

void ManageWiFiConnections::setUiState(UiState state) {
  if (_uiState == state) {
    return;
  }

  _uiState = state;

  if (config::WIFI_LOGGING_ENABLED) {
    Serial.print(F("[SMV] Wi-Fi UI state -> "));
    Serial.println(static_cast<unsigned int>(_uiState));
  }
}

}  // namespace smv
