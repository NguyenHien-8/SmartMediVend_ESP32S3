#include "DisplayManager.h"
#include "WiFiManager.h"

smv::DisplayManager displayManager;
smv::WiFiManager wifiManager;

void setup() {
  Serial.begin(115200);
  delay(50);

  Serial.println();
  Serial.println(F("=== SmartMediVend ESP32-S3 ==="));
  Serial.println(F("Wi-Fi: ESP32WiFiPortal v2.1.2"));
  Serial.println(F("TFT: ILI9341 320x240 SPI"));

  displayManager.begin();
  displayManager.showBootScreen();

  // Short one-time splash only. No long blocking delays are used in loop().
  delay(300);

  // Shows the "Connecting" screen before connectSaved() performs its bounded
  // boot connection attempt.
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
