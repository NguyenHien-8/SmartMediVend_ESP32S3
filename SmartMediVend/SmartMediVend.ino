#include <Arduino.h>

// Arduino-ESP32 gives loopTask an 8 KiB stack by default.  SmartMediVend runs
// the Opus encoder/decoder from loop(), and the codec alone is provisioned with
// a 24 KiB task stack by the upstream Xiaozhi firmware.  Leave additional
// headroom for WebSocket framing, the state machine and display rendering.
SET_LOOP_TASK_STACK_SIZE(32 * 1024);

#include "src/app/SmartMediVendApp.h"

smv::SmartMediVendApp app;

void setup() { app.begin(); }

void loop() { app.process(); }
