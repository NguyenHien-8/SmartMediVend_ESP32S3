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

// Portrait 240x320 layout. Horizontal dimensions are derived from the
// runtime TFT width so the UI never assumes a 320-pixel-wide canvas.
constexpr int16_t HEADER_HEIGHT = 46;
constexpr int16_t CARD_MARGIN_X = 10;
constexpr int16_t CARD_TOP = 52;
constexpr int16_t CARD_BOTTOM = 250;
constexpr int16_t FOOTER_TOP = 263;

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

  Serial.print(F("[SMV] TFT geometry: "));
  Serial.print(_tft.width());
  Serial.print('x');
  Serial.print(_tft.height());
  Serial.print(F(", rotation="));
  Serial.println(config::TFT_ROTATION);

  if (_tft.width() >= _tft.height()) {
    Serial.println(F("[SMV] WARNING: portrait UI expects width < height"));
  }

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

  const int16_t w = screenWidth();
  const int16_t h = screenHeight();
  const int16_t cx = w / 2;

  _tft.fillScreen(COLOR_BG);

  const int16_t logoW = 110;
  const int16_t logoH = 88;
  const int16_t logoX = cx - logoW / 2;
  const int16_t logoY = 56;

  _tft.fillRoundRect(logoX, logoY, logoW, logoH, 18, COLOR_PANEL);
  _tft.drawRoundRect(logoX, logoY, logoW, logoH, 18, COLOR_DIVIDER);

  // Minimal medical cross centered in the logo card.
  _tft.fillRoundRect(cx - 28, logoY + 34, 56, 20, 8, COLOR_ACCENT);
  _tft.fillRoundRect(cx - 10, logoY + 16, 20, 56, 8, COLOR_ACCENT);

  centeredText(F("SmartMediVend"), cx, 174, 2, COLOR_TEXT, COLOR_BG);
  centeredText(F("ESP32-S3"), cx, 207, 1, COLOR_MUTED, COLOR_BG);
  centeredText(F("Starting system..."), cx, 236, 1, COLOR_MUTED, COLOR_BG);

  _tft.drawFastHLine(20, h - 43, w - 40, COLOR_DIVIDER);
  centeredText(F("240x320 PORTRAIT"), cx, h - 29, 1, COLOR_MUTED, COLOR_BG);

  _forceRefresh = true;
}

void DisplayManager::process(const ManageWiFiConnections& wifi) {
  if (!_initialized) {
    return;
  }

  const uint32_t now = millis();
  const ManageWiFiConnections::UiState state = wifi.uiState();
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

  if (state == ManageWiFiConnections::UiState::Connected) {
    const uint8_t bars = rssiBars(wifi.rssi());
    if (bars != _lastRssiBars) {
      drawSignalBars(wifi.rssi());
      _lastRssiBars = bars;
    }
  }

  if ((state == ManageWiFiConnections::UiState::Connecting ||
       state == ManageWiFiConnections::UiState::PortalConnecting) &&
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
  const int16_t w = screenWidth();
  const int16_t cardW = w - 2 * CARD_MARGIN_X;
  const int16_t cardH = CARD_BOTTOM - CARD_TOP;

  _tft.fillScreen(COLOR_BG);
  _tft.fillRoundRect(CARD_MARGIN_X, CARD_TOP, cardW, cardH, 16, COLOR_PANEL);
  _tft.drawRoundRect(CARD_MARGIN_X, CARD_TOP, cardW, cardH, 16,
                     COLOR_DIVIDER);

  _tft.drawFastHLine(14, FOOTER_TOP, w - 28, COLOR_DIVIDER);
}

void DisplayManager::drawHeader(const ManageWiFiConnections& wifi) {
  const int16_t w = screenWidth();

  _tft.fillRect(0, 0, w, HEADER_HEIGHT, COLOR_BG);
  _tft.setTextWrap(false);

  _tft.setTextSize(2);
  _tft.setTextColor(COLOR_TEXT, COLOR_BG);
  _tft.setCursor(12, 8);
  _tft.print(F("SmartMediVend"));

  const uint16_t color = stateColor(wifi.uiState());
  _tft.fillCircle(15, 34, 3, color);

  _tft.setTextSize(1);
  _tft.setTextColor(color, COLOR_BG);
  _tft.setCursor(24, 31);
  _tft.print(badgeText(wifi.uiState()));
}

void DisplayManager::drawStatusCard(const ManageWiFiConnections& wifi) {
  const int16_t w = screenWidth();
  const int16_t cx = w / 2;
  const int16_t cardW = w - 2 * CARD_MARGIN_X;
  const int16_t cardH = CARD_BOTTOM - CARD_TOP;
  const uint16_t color = stateColor(wifi.uiState());

  _tft.fillRoundRect(CARD_MARGIN_X + 1, CARD_TOP + 1,
                     cardW - 2, cardH - 2, 15, COLOR_PANEL);

  drawWiFiIcon(color);
  _tft.setTextWrap(false);

  switch (wifi.uiState()) {
    case ManageWiFiConnections::UiState::Connecting:
      centeredText(F("Connecting Wi-Fi"), cx, 132, 2, COLOR_TEXT, COLOR_PANEL);
      centeredText(F("Checking saved network..."), cx, 160, 1,
                   COLOR_MUTED, COLOR_PANEL);
      centeredText(F("Auto Reconnect enabled"), cx, 188, 1,
                   COLOR_MUTED, COLOR_PANEL);
      clearSpinnerArea();
      drawSpinner(_spinnerStep, color);
      break;

    case ManageWiFiConnections::UiState::Connected: {
      centeredText(F("Wi-Fi connected"), cx, 132, 2, COLOR_TEXT, COLOR_PANEL);
      centeredText(wifi.ssid(), cx, 160, 1, COLOR_MUTED, COLOR_PANEL);

      String ipLine = F("IP: ");
      ipLine += wifi.localIP().toString();
      leftTextClipped(ipLine, 24, 187, 1, COLOR_MUTED, COLOR_PANEL, 31);

      leftTextClipped(F("System ready"), 24, 211, 1,
                      COLOR_ACCENT, COLOR_PANEL, 31);
      drawSignalBars(wifi.rssi());
      break;
    }

    case ManageWiFiConnections::UiState::Portal: {
      centeredText(F("Wi-Fi setup portal"), cx, 132, 2,
                   COLOR_TEXT, COLOR_PANEL);

      String apLine = F("AP: ");
      apLine += wifi.portalSSID();
      leftTextClipped(apLine, 24, 161, 1, COLOR_MUTED, COLOR_PANEL, 31);

      String openLine = F("Open: http://");
      openLine += wifi.portalIP().toString();
      leftTextClipped(openLine, 24, 187, 1, COLOR_BLUE, COLOR_PANEL, 31);

      centeredText(F("Select Wi-Fi in portal"), cx, 218, 1,
                   COLOR_MUTED, COLOR_PANEL);
      clearSpinnerArea();
      break;
    }

    case ManageWiFiConnections::UiState::PortalConnecting:
      centeredText(F("Applying Wi-Fi"), cx, 132, 2, COLOR_TEXT, COLOR_PANEL);
      centeredText(F("Testing new credentials..."), cx, 160, 1,
                   COLOR_MUTED, COLOR_PANEL);
      centeredText(F("Keep device powered"), cx, 188, 1,
                   COLOR_MUTED, COLOR_PANEL);
      clearSpinnerArea();
      drawSpinner(_spinnerStep, color);
      break;

    case ManageWiFiConnections::UiState::Error:
      centeredText(F("Wi-Fi setup error"), cx, 132, 2, COLOR_TEXT, COLOR_PANEL);
      leftTextClipped(wifi.lastError(), 24, 164, 1,
                      COLOR_RED, COLOR_PANEL, 31);
      centeredText(F("Release button and retry"), cx, 202, 1,
                   COLOR_MUTED, COLOR_PANEL);
      clearSpinnerArea();
      break;

    case ManageWiFiConnections::UiState::Offline:
      centeredText(F("Offline mode"), cx, 132, 2, COLOR_TEXT, COLOR_PANEL);
      centeredText(F("Device remains operational"), cx, 160, 1,
                   COLOR_MUTED, COLOR_PANEL);

      if (wifi.lastError().length() > 0) {
        leftTextClipped(wifi.lastError(), 24, 187, 1,
                        COLOR_YELLOW, COLOR_PANEL, 31);
      } else {
        centeredText(F("No active Wi-Fi"), cx, 187, 1,
                     COLOR_YELLOW, COLOR_PANEL);
      }

      centeredText(F("Hold SET WIFI for 2s"), cx, 218, 1,
                   COLOR_MUTED, COLOR_PANEL);
      clearSpinnerArea();
      break;

    case ManageWiFiConnections::UiState::Booting:
    default:
      centeredText(F("Starting..."), cx, 132, 2, COLOR_TEXT, COLOR_PANEL);
      centeredText(F("Initializing services"), cx, 160, 1,
                   COLOR_MUTED, COLOR_PANEL);
      clearSpinnerArea();
      break;
  }
}

void DisplayManager::drawFooter(const ManageWiFiConnections& wifi) {
  const int16_t w = screenWidth();
  const int16_t h = screenHeight();
  const int16_t footerY = FOOTER_TOP + 1;
  const int16_t footerH = h - footerY;

  _tft.fillRect(0, footerY, w, footerH, COLOR_BG);

  const uint8_t percent = wifi.configButtonHoldPercent();

  if (wifi.isConfigButtonPressed() && percent > 0) {
    centeredText(F("Hold to open Wi-Fi setup"), w / 2, 276, 1,
                 COLOR_TEXT, COLOR_BG);
    drawHoldProgress(percent);
  } else {
    centeredText(F("SET WIFI: hold 2s"), w / 2, 285, 1,
                 COLOR_MUTED, COLOR_BG);
  }
}

void DisplayManager::drawHoldProgress(uint8_t percent) {
  if (percent > 100) {
    percent = 100;
  }

  const int16_t w = screenWidth();
  const int16_t x = 16;
  const int16_t y = 301;
  const int16_t barW = w - 32;
  const int16_t barH = 10;

  _tft.fillRoundRect(x, y, barW, barH, 5, COLOR_PANEL_2);

  const int16_t fillW =
      static_cast<int16_t>((static_cast<uint32_t>(barW) * percent) / 100UL);

  if (fillW > 0) {
    _tft.fillRoundRect(x, y, fillW, barH, 5, COLOR_ACCENT);
  }
}

void DisplayManager::drawSignalBars(int32_t rssi) {
  const uint8_t bars = rssiBars(rssi);
  const int16_t w = screenWidth();

  const int16_t areaX = 118;
  const int16_t areaY = 203;
  const int16_t areaW = w - areaX - 22;
  const int16_t areaH = 39;

  _tft.fillRect(areaX, areaY, areaW, areaH, COLOR_PANEL);

  _tft.setTextSize(1);
  _tft.setTextColor(COLOR_MUTED, COLOR_PANEL);
  _tft.setCursor(areaX, 211);
  _tft.print(rssi);
  _tft.print(F(" dBm"));

  const int16_t firstBarX = w - 70;
  const int16_t baseY = 236;
  const int16_t barW = 7;
  const int16_t gap = 4;

  for (uint8_t i = 0; i < 4; ++i) {
    const int16_t barH = static_cast<int16_t>(7 + i * 6);
    const int16_t bx = firstBarX + i * (barW + gap);
    const int16_t by = baseY - barH;
    const uint16_t barColor = (i < bars) ? COLOR_ACCENT : COLOR_DIVIDER;

    _tft.fillRoundRect(bx, by, barW, barH, 2, barColor);
  }
}

void DisplayManager::drawSpinner(uint8_t step, uint16_t color) {
  static const int8_t dx[8] = {0, 7, 10, 7, 0, -7, -10, -7};
  static const int8_t dy[8] = {-10, -7, 0, 7, 10, 7, 0, -7};

  const int16_t cx = centerX();
  const int16_t cy = 220;

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
  _tft.fillRect(centerX() - 18, 202, 36, 36, COLOR_PANEL);
}

void DisplayManager::drawWiFiIcon(uint16_t color) {
  const int16_t cx = centerX();
  const int16_t iconY = 66;
  const int16_t boxW = 60;
  const int16_t boxH = 52;
  const int16_t boxX = cx - boxW / 2;

  _tft.fillRoundRect(boxX, iconY, boxW, boxH, 12, COLOR_PANEL_2);
  _tft.drawRoundRect(boxX, iconY, boxW, boxH, 12, color);

  const int16_t glyphY = iconY + 29;
  _tft.drawCircle(cx, glyphY, 3, color);
  _tft.drawCircle(cx, glyphY, 10, color);
  _tft.drawCircle(cx, glyphY, 18, color);

  // Hide the lower half of the circles, leaving clean Wi-Fi arcs.
  _tft.fillRect(boxX + 7, glyphY, boxW - 14, 18, COLOR_PANEL_2);
  _tft.fillCircle(cx, glyphY + 1, 3, color);
}

void DisplayManager::centeredText(const String& text,
                                  int16_t centerXValue,
                                  int16_t y,
                                  uint8_t textSize,
                                  uint16_t color,
                                  uint16_t backgroundColor) {
  int16_t x1 = 0;
  int16_t y1 = 0;
  uint16_t textW = 0;
  uint16_t textH = 0;

  _tft.setTextSize(textSize);
  _tft.setTextWrap(false);
  _tft.getTextBounds(text, 0, 0, &x1, &y1, &textW, &textH);

  int16_t x = centerXValue - static_cast<int16_t>(textW / 2U);
  if (x < 2) {
    x = 2;
  }

  _tft.setTextColor(color, backgroundColor);
  _tft.setCursor(x, y);
  _tft.print(text);
}

void DisplayManager::leftTextClipped(const String& text,
                                     int16_t x,
                                     int16_t y,
                                     uint8_t textSize,
                                     uint16_t color,
                                     uint16_t backgroundColor,
                                     uint8_t maxChars) {
  String clipped = text;

  if (clipped.length() > maxChars && maxChars >= 3) {
    clipped.remove(maxChars - 3);
    clipped += F("...");
  }

  _tft.setTextWrap(false);
  _tft.setTextSize(textSize);
  _tft.setTextColor(color, backgroundColor);
  _tft.setCursor(x, y);
  _tft.print(clipped);
}

const char* DisplayManager::badgeText(
    ManageWiFiConnections::UiState state) const {
  switch (state) {
    case ManageWiFiConnections::UiState::Connected:
      return "ONLINE";
    case ManageWiFiConnections::UiState::Portal:
    case ManageWiFiConnections::UiState::PortalConnecting:
      return "SETUP";
    case ManageWiFiConnections::UiState::Connecting:
      return "CONNECTING";
    case ManageWiFiConnections::UiState::Error:
      return "ERROR";
    case ManageWiFiConnections::UiState::Offline:
      return "OFFLINE";
    case ManageWiFiConnections::UiState::Booting:
    default:
      return "BOOT";
  }
}

uint16_t DisplayManager::stateColor(
    ManageWiFiConnections::UiState state) const {
  switch (state) {
    case ManageWiFiConnections::UiState::Connected:
      return COLOR_ACCENT;
    case ManageWiFiConnections::UiState::Portal:
      return COLOR_BLUE;
    case ManageWiFiConnections::UiState::PortalConnecting:
      return COLOR_ACCENT;
    case ManageWiFiConnections::UiState::Connecting:
      return COLOR_BLUE;
    case ManageWiFiConnections::UiState::Error:
      return COLOR_RED;
    case ManageWiFiConnections::UiState::Offline:
      return COLOR_YELLOW;
    case ManageWiFiConnections::UiState::Booting:
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
