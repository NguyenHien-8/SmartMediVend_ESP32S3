# SmartMediVend ESP32-S3

This package implements the first SmartMediVend firmware layer:

- ESP32-S3
- ESP32WiFiPortal **2.1.2**
- ILI9341 2.4" SPI TFT, 320x240
- Non-blocking on-demand Wi-Fi captive portal
- Active-HIGH SET WIFI button with external 4.7 kOhm pull-down
- Hold button for **2 seconds** to open the portal
- TFT status UI with spinner, Wi-Fi signal bars and hold-progress animation
- Structure intended to be extended later with Xiaozhi, INMP441, MAX98357A,
  medicine selection and vending/actuator modules.

## GPIO mapping

| Function | ESP32-S3 GPIO |
|---|---:|
| TFT CS | 10 |
| TFT RST | 14 |
| TFT DC | 9 |
| TFT MOSI | 11 |
| TFT SCLK | 12 |
| TFT BL | 13 |
| SET WIFI button | 18 |

> The duplicated `#define TFT_BL 18` from the requirement was interpreted as
> `BT_SETWIFI = 18`. TFT backlight remains GPIO13.

Button wiring:

- GPIO18 -> button -> 3.3 V
- 4.7 kOhm resistor from GPIO18 to GND
- Pressed = HIGH
- Released = LOW

## Required Arduino libraries

Install:

1. **ESP32WiFiPortal 2.1.2**  
   https://github.com/NguyenHien-8/ESP32WiFiPortal
2. **Adafruit GFX Library**
3. **Adafruit ILI9341**
4. **Adafruit BusIO** (normally installed automatically as an Adafruit GFX dependency)

Select an ESP32-S3 board in Arduino IDE and compile `SmartMediVend.ino`.

## Firmware structure

```text
SmartMediVend/
├── SmartMediVend.ino
├── AppConfig.h
├── HardwarePins.h
├── WiFiManager.h
├── WiFiManager.cpp
├── DisplayManager.h
├── DisplayManager.cpp
└── README.md
```

### `SmartMediVend.ino`

Only coordinates subsystems. Keep it small.

### `WiFiManager`

Owns:

- ESP32WiFiPortal
- saved Wi-Fi boot connection
- ESP32WiFiPortal Auto Reconnect
- asynchronous captive portal
- 2-second SET WIFI button debounce/hold detector
- lightweight Wi-Fi data cached for the display

It does **not** implement a second reconnect algorithm. Reconnect/backoff stays
inside ESP32WiFiPortal 2.1.2.

### `DisplayManager`

Owns the TFT only. It uses partial/redraw-on-change rendering rather than
continually clearing the full screen, reducing flicker and SPI traffic.

## Wi-Fi behavior

1. Power on.
2. Show boot UI.
3. Try saved credentials for a bounded 12 s connection attempt.
4. If connected: show SSID, IP and RSSI.
5. If unavailable: remain in offline mode.
6. Hold SET WIFI for 2 s:
   - button progress appears on TFT;
   - `startConfigPortalAsync()` opens `SmartMediVend-Setup`;
   - TFT shows AP name and portal IP;
   - main `loop()` remains cooperative.
7. When new credentials connect successfully, ESP32WiFiPortal saves them and
   the screen returns to ONLINE.
8. If Wi-Fi later drops, ESP32WiFiPortal's own Auto Reconnect handles recovery.

## Portal password

Default development password:

```text
SMV-Setup-2026
```

Change `WIFI_PORTAL_PASSWORD` in `AppConfig.h` before deployment.

## Important TFT note

This package assumes the 2.4" TFT uses an **ILI9341** controller. If your
specific "2.4 inch V1.3" board uses ST7789 or another controller, keep
`WiFiManager` unchanged and replace only the driver-specific part of
`DisplayManager`.

The built-in Adafruit GFX font is used intentionally for a small, stable
baseline, so TFT text is English/ASCII. Vietnamese Unicode fonts can be added
later without changing the Wi-Fi architecture.
