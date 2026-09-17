#pragma once

#include <Arduino.h>
#include <SPI.h>

#include "src/network/WiFiService.h"
#include "src/vendor/Adafruit/Adafruit_GFX.h"
#include "src/vendor/Adafruit/Adafruit_ST7789.h"

namespace smv {

class DisplayManager {
 public:
  DisplayManager();

  bool begin();
  void showBootScreen();

  // Non-blocking UI service. Redraws only changed regions.
  void process(const WiFiService& wifi);

  // Use after a subsystem changes the whole screen in future.
  void forceRefresh();

 private:
  void drawStaticFrame();
  void drawHeader(const WiFiService& wifi);
  void drawStatusCard(const WiFiService& wifi);
  void drawFooter(const WiFiService& wifi);
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

  const char* badgeText(WiFiService::UiState state) const;
  uint16_t stateColor(WiFiService::UiState state) const;
  uint8_t rssiBars(int32_t rssi) const;

  int16_t screenWidth() const { return _tft.width(); }
  int16_t screenHeight() const { return _tft.height(); }
  int16_t centerX() const { return screenWidth() / 2; }

  Adafruit_ST7789 _tft;

  bool _initialized = false;
  bool _forceRefresh = true;

  WiFiService::UiState _lastState = WiFiService::UiState::Booting;
  uint8_t _lastHoldPercent = 255;
  uint8_t _lastRssiBars = 255;
  uint8_t _spinnerStep = 0;

  uint32_t _lastAnimationAt = 0;
};

}  // namespace smv
