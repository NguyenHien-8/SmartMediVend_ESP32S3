#pragma once

#include <Arduino.h>
#include <ESP32WiFiPortal.h>
#include <IPAddress.h>
#include <WiFi.h>

namespace smv {

class ManageWiFiConnections {
 public:
  enum class UiState : uint8_t {
    Booting = 0,
    Connecting,
    Connected,
    Offline,
    Portal,
    PortalConnecting,
    Error
  };

  ManageWiFiConnections() = default;

  // Tries saved Wi-Fi once at boot. ESP32WiFiPortal keeps its own bounded,
  // cooperative Auto Reconnect policy after that.
  bool begin();

  // Must be called frequently from loop().
  void process();

  // Starts the captive portal without blocking loop().
  bool requestConfigPortal();

  UiState uiState() const { return _uiState; }
  bool isConnected() const { return _portal.isConnected(); }
  bool isPortalActive() const { return _portal.isPortalActive(); }

  bool isConfigButtonPressed() const { return _buttonStable; }
  uint8_t configButtonHoldPercent() const;

  const String& ssid() const { return _ssid; }
  const String& lastError() const { return _lastError; }

  IPAddress localIP() const { return _localIP; }
  IPAddress portalIP() const { return _portalIP; }
  const String& portalSSID() const { return _portalSSID; }

  int32_t rssi() const { return _rssi; }

 private:
  void processConfigButton(uint32_t now);
  void syncUiState(uint32_t now, bool forceRefresh);
  void refreshConnectedTelemetry(uint32_t now, bool forceRefresh);
  void refreshPortalTelemetry();
  void setUiState(UiState state);

  ESP32WiFiPortal _portal;

  UiState _uiState = UiState::Booting;

  String _ssid;
  String _portalSSID;
  String _lastError;

  IPAddress _localIP;
  IPAddress _portalIP;

  int32_t _rssi = -127;

  bool _buttonRaw = false;
  bool _buttonStable = false;
  bool _buttonHandled = false;
  uint32_t _buttonRawChangedAt = 0;
  uint32_t _buttonPressedAt = 0;

  bool _portalStartError = false;
  uint32_t _portalStartErrorAt = 0;

  uint32_t _lastTelemetryAt = 0;
};

}  // namespace smv
