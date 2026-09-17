#pragma once

#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <Arduino.h>
#include <SPI.h>

#include "ManageWiFiConnections.h"

namespace smv {

class DisplayManager {
 public:
  DisplayManager();

  bool begin();
  void showBootScreen();

  // Non-blocking UI service. Redraws only changed regions.
  void process(const ManageWiFiConnections& wifi);

  // Use after a subsystem changes the whole screen in future.
  void forceRefresh();

 private:
  void drawStaticFrame();
  void drawHeader(const ManageWiFiConnections& wifi);
  void drawStatusCard(const ManageWiFiConnections& wifi);
  void drawFooter(const ManageWiFiConnections& wifi);
  void drawHoldProgress(uint8_t percent);
  void drawSignalBars(int32_t rssi);
  void drawSpinner(uint8_t step, uint16_t color);
  void clearSpinnerArea();
  void drawWiFiIcon(uint16_t color);

  void centeredText(const String& text,
                    int16_t centerX,
                    int16_t y,
                    uint8_t textSize,
                    uint16_t color,
                    uint16_t backgroundColor);

  void leftTextClipped(const String& text,
                       int16_t x,
                       int16_t y,
                       uint8_t textSize,
                       uint16_t color,
                       uint16_t backgroundColor,
                       uint8_t maxChars);

  const char* badgeText(ManageWiFiConnections::UiState state) const;
  uint16_t stateColor(ManageWiFiConnections::UiState state) const;
  uint8_t rssiBars(int32_t rssi) const;

  int16_t screenWidth() const { return _tft.width(); }
  int16_t screenHeight() const { return _tft.height(); }
  int16_t centerX() const { return screenWidth() / 2; }

  Adafruit_ILI9341 _tft;

  bool _initialized = false;
  bool _forceRefresh = true;

  ManageWiFiConnections::UiState _lastState =
      ManageWiFiConnections::UiState::Booting;
  uint8_t _lastHoldPercent = 255;
  uint8_t _lastRssiBars = 255;
  uint8_t _spinnerStep = 0;

  uint32_t _lastAnimationAt = 0;
};

}  // namespace smv
