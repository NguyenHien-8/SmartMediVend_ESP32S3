#include "DisplayManager.h"
#include "src/network/WiFiService.h"

smv::DisplayManager displayManager;
smv::WiFiService wifiManager;

void setup() {
  Serial.begin(115200);
  delay(50);

  Serial.println();
  Serial.println(F("=== SmartMediVend ESP32-S3 ==="));
  Serial.println(F("Wi-Fi: ESP32WiFiPortal v2.1.2"));
  Serial.println(F("TFT: ST7789 240x320 SPI - Portrait UI"));

  displayManager.begin();
  displayManager.showBootScreen();

  // Short one-time splash only. No long blocking delays are used in loop().
  delay(300);

  displayManager.forceRefresh();

  wifiManager.begin();

  // Repaint immediately with Connected / Offline result.
  displayManager.forceRefresh();
  displayManager.process(wifiManager);
}

void loop() {
  // Keep this loop cooperative. Future Xiaozhi/audio/dispensing services can
  // be added beside these calls without changing the Wi-Fi/TFT architecture.
  wifiManager.process();
  displayManager.process(wifiManager);

  // Small yield for ESP32 Wi-Fi/RTOS tasks; never use long delays here.
  delay(1);
}
