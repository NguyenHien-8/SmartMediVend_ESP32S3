#pragma once

#include <Arduino.h>
#include <IPAddress.h>
#include <WiFi.h>

#include "../app/ButtonController.h"
#include "../vendor/ESP32WiFiPortal/ESP32WiFiPortal.h"

namespace smv {

class WiFiService {
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

  WiFiService();

  bool begin();
  void process();
  bool requestConfigPortal();

  // Returns a pending physical-button gesture exactly once. LongPress is
  // also handled locally by opening the portal; ShortPress belongs to the
  // application conversation controller.
  ButtonEvent takeButtonEvent();

  UiState uiState() const { return uiState_; }
  bool isConnected() const { return portal_.isConnected(); }
  bool isPortalActive() const { return portal_.isPortalActive(); }
  bool isConfigButtonPressed() const { return button_.isPressed(); }
  uint8_t configButtonHoldPercent() const;

  const String& ssid() const { return ssid_; }
  const String& lastError() const { return lastError_; }
  IPAddress localIP() const { return localIP_; }
  IPAddress portalIP() const { return portalIP_; }
  const String& portalSSID() const { return portalSSID_; }
  int32_t rssi() const { return rssi_; }

 private:
  void syncUiState(uint32_t now, bool forceRefresh);
  void refreshConnectedTelemetry(uint32_t now, bool forceRefresh);
  void refreshPortalTelemetry();
  void setUiState(UiState state);

  ESP32WiFiPortal portal_;
  ButtonController button_;
  ButtonEvent pendingButtonEvent_ = ButtonEvent::None;
  UiState uiState_ = UiState::Booting;

  String ssid_;
  String portalSSID_;
  String lastError_;
  IPAddress localIP_;
  IPAddress portalIP_;
  int32_t rssi_ = -127;

  bool portalStartError_ = false;
  uint32_t portalStartErrorAt_ = 0;
  uint32_t lastTelemetryAt_ = 0;
};

}  // namespace smv
