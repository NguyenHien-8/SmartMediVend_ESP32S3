# Vendored dependencies

The firmware includes source snapshots so a clean Arduino build does not rely
on mutable global libraries. No nested Git metadata is included.

| Component | Upstream | Revision | License | Local layout/changes |
|---|---|---|---|---|
| ESP32WiFiPortal 2.1.2 | https://github.com/tzapu/ESP32WiFiPortal | `116d774fef4bd9ac0344813f54a516678a68daf3` | Apache-2.0 | Required source, metadata, and license copied into `ESP32WiFiPortal/`; no behavior changes. |
| ArduinoJson 7.x | https://github.com/bblanchon/ArduinoJson | `1cd478f48e7c4cd142a0bd26ee17d63d70d04e3f` | MIT | Header-only `src` snapshot flattened into `ArduinoJson/`; no behavior changes. |
| Adafruit GFX 1.12.6 | https://github.com/adafruit/Adafruit-GFX-Library | `ac6d7c3869a693d406f77b9bfcd486b0673169f0` | BSD-3-Clause | Required root sources flattened into `Adafruit/`. |
| Adafruit ST7735/ST7789 1.11.0 | https://github.com/adafruit/Adafruit-ST7735-Library | `62112b90eddcb2ecc51f474e9fe98b68eb26cb2a` | MIT | ST77xx/ST7789 sources flattened into `Adafruit/`. |
| Adafruit BusIO 1.17.4 | https://github.com/adafruit/Adafruit_BusIO | `3b8364267c3ee6e16bad91bc2101aefbd5b5915f` | MIT | Required SPI/I2C/register sources flattened into `Adafruit/`. |
| arduinoWebSockets | https://github.com/Links2004/arduinoWebSockets | `e3edca4907e8524856fbc38a6bb00eb6073c982e` | LGPL-2.1 | `src`, metadata, and license copied into `arduinoWebSockets/`; no behavior changes. |
| libopus Arduino 1.3.2 | https://github.com/pschatzmann/arduino-libopus | `bae0f8570b03071d0101445059e7178bed8bd144` | Opus BSD-style license | Fixed-point Arduino port plus Opus license copied into `arduino-libopus/`; unused libogg/liboggz container trees are excluded. |

ESP32 Arduino core APIs (Wi-Fi, TLS, HTTP, NVS, I2S, FreeRTOS) are supplied by
the pinned board-platform installation documented in the build report.

Local include-only changes replace a few library-style angle-bracket includes
with same-directory quoted includes. This prevents an Arduino installation's
global libraries from shadowing these reviewed snapshots.

The sketch-root `opus*.h` files are forwarding headers required because the
Arduino sketch builder adds the sketch root, but not arbitrary nested vendor
directories, to every recursively compiled C source include path.
