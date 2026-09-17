#include "DisplayManager.h"

#include "AppConfig.h"
#include "HardwarePins.h"

namespace smv {

namespace {
constexpr uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
  return static_cast<uint16_t>(
      ((static_cast<uint16_t>(r) & 0xF8U) << 8) |
      ((static_cast<uint16_t>(g) & 0xFCU) << 3) |
      (static_cast<uint16_t>(b) >> 3));
}

constexpr uint16_t COLOR_BG      = rgb565(8, 13, 22);
constexpr uint16_t COLOR_PANEL   = rgb565(17, 26, 40);
constexpr uint16_t COLOR_PANEL_2 = rgb565(24, 36, 54);
constexpr uint16_t COLOR_TEXT    = rgb565(238, 244, 255);
constexpr uint16_t COLOR_MUTED   = rgb565(139, 156, 179);
constexpr uint16_t COLOR_ACCENT  = rgb565(55, 205, 173);
constexpr uint16_t COLOR_BLUE    = rgb565(80, 150, 255);
constexpr uint16_t COLOR_YELLOW  = rgb565(255, 193, 74);
constexpr uint16_t COLOR_RED     = rgb565(255, 91, 105);
constexpr uint16_t COLOR_DIVIDER = rgb565(45, 61, 82);

bool elapsed(uint32_t now, uint32_t since, uint32_t interval) {
  return static_cast<uint32_t>(now - since) >= interval;
}
}  // namespace

DisplayManager::DisplayManager()
    : _tft(&SPI, pins::TFT_DC, pins::TFT_CS, pins::TFT_RST) {}

bool DisplayManager::begin() {
  pinMode(pins::TFT_BL, OUTPUT);

  const uint8_t backlightOff =
      config::TFT_BACKLIGHT_ACTIVE_HIGH ? LOW : HIGH;
  const uint8_t backlightOn =
      config::TFT_BACKLIGHT_ACTIVE_HIGH ? HIGH : LOW;

  digitalWrite(pins::TFT_BL, backlightOff);

  // No MISO is required for this write-only TFT use case.
  SPI.begin(pins::TFT_SCLK, -1, pins::TFT_MOSI, pins::TFT_CS);

  _tft.begin(config::TFT_SPI_FREQUENCY_HZ);
  _tft.setRotation(config::TFT_ROTATION);
  _tft.fillScreen(COLOR_BG);

  digitalWrite(pins::TFT_BL, backlightOn);

  _initialized = true;
  _forceRefresh = true;
  return true;
}

void DisplayManager::showBootScreen() {
  if (!_initialized) {
    return;
  }

  _tft.fillScreen(COLOR_BG);

  _tft.fillRoundRect(96, 56, 128, 70, 18, COLOR_PANEL);
  _tft.drawRoundRect(96, 56, 128, 70, 18, COLOR_DIVIDER);

  // Minimal capsule / medical mark.
  _tft.fillRoundRect(135, 72, 50, 18, 9, COLOR_ACCENT);
  _tft.fillRoundRect(151, 62, 18, 38, 9, COLOR_ACCENT);

  centeredText(F("SmartMediVend"), 160, 145, 2, COLOR_TEXT);
  centeredText(F("ESP32-S3"), 160, 172, 1, COLOR_MUTED);
  centeredText(F("Starting system..."), 160, 203, 1, COLOR_MUTED);

  _forceRefresh = true;
}

void DisplayManager::process(const WiFiManager& wifi) {
  if (!_initialized) {
    return;
  }

  const uint32_t now = millis();
  const WiFiManager::UiState state = wifi.uiState();
  const bool stateChanged = (state != _lastState);

  if (_forceRefresh || stateChanged) {
    drawStaticFrame();
    drawHeader(wifi);
    drawStatusCard(wifi);
    drawFooter(wifi);

    _lastState = state;
    _lastHoldPercent = wifi.configButtonHoldPercent();
    _lastRssiBars = rssiBars(wifi.rssi());
    _spinnerStep = 0;
    _lastAnimationAt = now;
    _forceRefresh = false;
  }

  const uint8_t holdPercent = wifi.configButtonHoldPercent();
  if (holdPercent != _lastHoldPercent) {
    drawFooter(wifi);
    _lastHoldPercent = holdPercent;
  }

  if (state == WiFiManager::UiState::Connected) {
    const uint8_t bars = rssiBars(wifi.rssi());
    if (bars != _lastRssiBars) {
      drawSignalBars(wifi.rssi());
      _lastRssiBars = bars;
    }
  }

  if ((state == WiFiManager::UiState::Connecting ||
       state == WiFiManager::UiState::PortalConnecting) &&
      elapsed(now, _lastAnimationAt, config::TFT_ANIMATION_INTERVAL_MS)) {
    _lastAnimationAt = now;
    _spinnerStep = static_cast<uint8_t>((_spinnerStep + 1U) % 8U);
    drawSpinner(_spinnerStep, stateColor(state));
  }
}

void DisplayManager::forceRefresh() {
  _forceRefresh = true;
}

void DisplayManager::drawStaticFrame() {
  _tft.fillScreen(COLOR_BG);

  _tft.fillRoundRect(10, 42, 300, 158, 16, COLOR_PANEL);
  _tft.drawRoundRect(10, 42, 300, 158, 16, COLOR_DIVIDER);

  _tft.drawFastHLine(18, 210, 284, COLOR_DIVIDER);
}

void DisplayManager::drawHeader(const WiFiManager& wifi) {
  _tft.fillRect(0, 0, 320, 38, COLOR_BG);

  _tft.setTextWrap(false);
  _tft.setTextSize(2);
  _tft.setTextColor(COLOR_TEXT, COLOR_BG);
  _tft.setCursor(12, 11);
  _tft.print(F("SmartMediVend"));

  const uint16_t color = stateColor(wifi.uiState());

  _tft.fillCircle(241, 18, 4, color);
  _tft.setTextSize(1);
  _tft.setTextColor(color, COLOR_BG);
  _tft.setCursor(250, 15);
  _tft.print(badgeText(wifi.uiState()));
}

void DisplayManager::drawStatusCard(const WiFiManager& wifi) {
  _tft.fillRoundRect(11, 43, 298, 156, 15, COLOR_PANEL);

  const uint16_t color = stateColor(wifi.uiState());

  _tft.fillRoundRect(24, 58, 44, 44, 12, COLOR_PANEL_2);
  _tft.drawRoundRect(24, 58, 44, 44, 12, color);

  // Compact Wi-Fi glyph.
  _tft.drawCircle(46, 83, 3, color);
  _tft.drawCircle(46, 83, 9, color);
  _tft.drawCircle(46, 83, 16, color);
  // Cover lower halves to turn circles into arcs.
  _tft.fillRect(25, 83, 43, 20, COLOR_PANEL_2);
  _tft.fillCircle(46, 84, 3, color);

  _tft.setTextWrap(false);

  switch (wifi.uiState()) {
    case WiFiManager::UiState::Connecting:
      leftTextClipped(F("Connecting Wi-Fi"), 82, 61, 2, COLOR_TEXT, 22);
      leftTextClipped(F("Checking saved network..."), 82, 88, 1,
                      COLOR_MUTED, 32);
      leftTextClipped(F("Auto Reconnect stays enabled"), 24, 125, 1,
                      COLOR_MUTED, 38);
      clearSpinnerArea();
      drawSpinner(_spinnerStep, color);
      break;

    case WiFiManager::UiState::Connected: {
      leftTextClipped(F("Wi-Fi connected"), 82, 61, 2, COLOR_TEXT, 22);
      leftTextClipped(wifi.ssid(), 82, 88, 1, COLOR_MUTED, 30);

      String ipLine = F("IP: ");
      ipLine += wifi.localIP().toString();
      leftTextClipped(ipLine, 24, 125, 1, COLOR_MUTED, 38);

      leftTextClipped(F("Ready for SmartMediVend services"), 24, 151, 1,
                      COLOR_ACCENT, 40);

      drawSignalBars(wifi.rssi());
      break;
    }

    case WiFiManager::UiState::Portal:
      leftTextClipped(F("Wi-Fi setup portal"), 82, 61, 2, COLOR_TEXT, 23);

      {
        String apLine = F("AP: ");
        apLine += wifi.portalSSID();
        leftTextClipped(apLine, 82, 88, 1, COLOR_MUTED, 32);

        String openLine = F("Open: http://");
        openLine += wifi.portalIP().toString();
        leftTextClipped(openLine, 24, 125, 1, COLOR_BLUE, 39);
      }

      leftTextClipped(F("Choose a network in the captive portal"), 24, 151, 1,
                      COLOR_MUTED, 42);
      clearSpinnerArea();
      break;

    case WiFiManager::UiState::PortalConnecting:
      leftTextClipped(F("Applying Wi-Fi"), 82, 61, 2, COLOR_TEXT, 22);
      leftTextClipped(F("Testing new credentials..."), 82, 88, 1,
                      COLOR_MUTED, 32);
      leftTextClipped(F("Keep the device powered on"), 24, 125, 1,
                      COLOR_MUTED, 38);
      clearSpinnerArea();
      drawSpinner(_spinnerStep, color);
      break;

    case WiFiManager::UiState::Error:
      leftTextClipped(F("Wi-Fi setup error"), 82, 61, 2, COLOR_TEXT, 22);
      leftTextClipped(wifi.lastError(), 24, 112, 1, COLOR_RED, 43);
      leftTextClipped(F("Release button and try again"), 24, 151, 1,
                      COLOR_MUTED, 40);
      clearSpinnerArea();
      break;

    case WiFiManager::UiState::Offline:
      leftTextClipped(F("Offline mode"), 82, 61, 2, COLOR_TEXT, 22);
      leftTextClipped(F("Device remains operational"), 82, 88, 1,
                      COLOR_MUTED, 32);

      if (wifi.lastError().length() > 0) {
        leftTextClipped(wifi.lastError(), 24, 125, 1, COLOR_YELLOW, 43);
      } else {
        leftTextClipped(F("No active Wi-Fi connection"), 24, 125, 1,
                        COLOR_YELLOW, 38);
      }

      leftTextClipped(F("Hold SET WIFI for 2 seconds"), 24, 151, 1,
                      COLOR_MUTED, 40);
      clearSpinnerArea();
      break;

    case WiFiManager::UiState::Booting:
    default:
      leftTextClipped(F("Starting..."), 82, 61, 2, COLOR_TEXT, 22);
      leftTextClipped(F("Initializing services"), 82, 88, 1,
                      COLOR_MUTED, 32);
      clearSpinnerArea();
      break;
  }
}

void DisplayManager::drawFooter(const WiFiManager& wifi) {
  _tft.fillRect(0, 211, 320, 29, COLOR_BG);

  const uint8_t percent = wifi.configButtonHoldPercent();

  if (wifi.isConfigButtonPressed() && percent > 0) {
    _tft.setTextSize(1);
    _tft.setTextColor(COLOR_TEXT, COLOR_BG);
    _tft.setCursor(18, 216);
    _tft.print(F("Hold to open Wi-Fi setup"));

    drawHoldProgress(percent);
  } else {
    _tft.setTextSize(1);
    _tft.setTextColor(COLOR_MUTED, COLOR_BG);
    _tft.setCursor(18, 220);
    _tft.print(F("SET WIFI: hold 2s"));
  }
}

void DisplayManager::drawHoldProgress(uint8_t percent) {
  if (percent > 100) {
    percent = 100;
  }

  const int16_t x = 176;
  const int16_t y = 217;
  const int16_t w = 126;
  const int16_t h = 8;

  _tft.fillRoundRect(x, y, w, h, 4, COLOR_PANEL_2);

  const int16_t fillW =
      static_cast<int16_t>((static_cast<uint32_t>(w) * percent) / 100UL);

  if (fillW > 0) {
    _tft.fillRoundRect(x, y, fillW, h, 4, COLOR_ACCENT);
  }
}

void DisplayManager::drawSignalBars(int32_t rssi) {
  const uint8_t bars = rssiBars(rssi);

  const int16_t x = 246;
  const int16_t y = 152;
  const int16_t barW = 8;
  const int16_t gap = 4;

  _tft.fillRect(238, 120, 60, 62, COLOR_PANEL);

  _tft.setTextSize(1);
  _tft.setTextColor(COLOR_MUTED, COLOR_PANEL);
  _tft.setCursor(242, 122);
  _tft.print(rssi);
  _tft.print(F(" dBm"));

  for (uint8_t i = 0; i < 4; ++i) {
    const int16_t h = static_cast<int16_t>(7 + i * 7);
    const int16_t bx = x + i * (barW + gap);
    const int16_t by = y + 25 - h;

    const uint16_t color =
        (i < bars) ? COLOR_ACCENT : COLOR_DIVIDER;

    _tft.fillRoundRect(bx, by, barW, h, 2, color);
  }
}

void DisplayManager::drawSpinner(uint8_t step, uint16_t color) {
  static const int8_t dx[8] = {0, 7, 10, 7, 0, -7, -10, -7};
  static const int8_t dy[8] = {-10, -7, 0, 7, 10, 7, 0, -7};

  const int16_t cx = 274;
  const int16_t cy = 165;

  clearSpinnerArea();

  for (uint8_t i = 0; i < 8; ++i) {
    const uint8_t distance =
        static_cast<uint8_t>((i + 8U - step) % 8U);

    uint16_t dotColor = COLOR_DIVIDER;
    uint8_t radius = 2;

    if (distance == 0) {
      dotColor = color;
      radius = 4;
    } else if (distance <= 2) {
      dotColor = COLOR_MUTED;
      radius = 3;
    }

    _tft.fillCircle(cx + dx[i], cy + dy[i], radius, dotColor);
  }
}

void DisplayManager::clearSpinnerArea() {
  _tft.fillRect(258, 149, 34, 34, COLOR_PANEL);
}

void DisplayManager::centeredText(const String& text,
                                  int16_t centerX,
                                  int16_t y,
                                  uint8_t textSize,
                                  uint16_t color) {
  int16_t x1 = 0;
  int16_t y1 = 0;
  uint16_t w = 0;
  uint16_t h = 0;

  _tft.setTextSize(textSize);
  _tft.setTextWrap(false);
  _tft.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);

  const int16_t x =
      centerX - static_cast<int16_t>(w / 2U);

  _tft.setTextColor(color, COLOR_BG);
  _tft.setCursor(x, y);
  _tft.print(text);
}

void DisplayManager::leftTextClipped(const String& text,
                                     int16_t x,
                                     int16_t y,
                                     uint8_t textSize,
                                     uint16_t color,
                                     uint8_t maxChars) {
  String clipped = text;

  if (clipped.length() > maxChars && maxChars >= 3) {
    clipped.remove(maxChars - 3);
    clipped += F("...");
  }

  _tft.setTextWrap(false);
  _tft.setTextSize(textSize);
  _tft.setTextColor(color, COLOR_PANEL);
  _tft.setCursor(x, y);
  _tft.print(clipped);
}

const char* DisplayManager::badgeText(WiFiManager::UiState state) const {
  switch (state) {
    case WiFiManager::UiState::Connected:
      return "ONLINE";
    case WiFiManager::UiState::Portal:
    case WiFiManager::UiState::PortalConnecting:
      return "SETUP";
    case WiFiManager::UiState::Connecting:
      return "CONNECT";
    case WiFiManager::UiState::Error:
      return "ERROR";
    case WiFiManager::UiState::Offline:
      return "OFFLINE";
    case WiFiManager::UiState::Booting:
    default:
      return "BOOT";
  }
}

uint16_t DisplayManager::stateColor(WiFiManager::UiState state) const {
  switch (state) {
    case WiFiManager::UiState::Connected:
      return COLOR_ACCENT;
    case WiFiManager::UiState::Portal:
      return COLOR_BLUE;
    case WiFiManager::UiState::PortalConnecting:
      return COLOR_ACCENT;
    case WiFiManager::UiState::Connecting:
      return COLOR_BLUE;
    case WiFiManager::UiState::Error:
      return COLOR_RED;
    case WiFiManager::UiState::Offline:
      return COLOR_YELLOW;
    case WiFiManager::UiState::Booting:
    default:
      return COLOR_MUTED;
  }
}

uint8_t DisplayManager::rssiBars(int32_t rssi) const {
  if (rssi >= -55) return 4;
  if (rssi >= -67) return 3;
  if (rssi >= -75) return 2;
  if (rssi >= -85) return 1;
  return 0;
}

}  // namespace smv
