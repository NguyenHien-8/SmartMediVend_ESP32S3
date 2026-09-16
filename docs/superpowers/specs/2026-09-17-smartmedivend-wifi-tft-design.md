# SmartMediVend Wi-Fi Portal and TFT Design

## Purpose

Build the first production-oriented firmware foundation for SmartMediVend on an ESP32-S3. This phase manages saved Wi-Fi, an on-demand captive portal, and a responsive 2.4-inch TFT UI. It deliberately excludes Xiaozhi, audio, diagnosis, medicine recommendation, inventory, payment, and vending mechanisms.

The design favors cooperative runtime behavior, bounded retries, fixed-size application data, minimal redraws, and explicit ownership of all Wi-Fi APIs so the device can run continuously without reconnect storms or UI stalls.

## Confirmed Hardware

- MCU family: ESP32-S3.
- Display: 2.4-inch SPI TFT, 240 x 320 pixels, ILI9341 controller, portrait orientation.
- Touch and microSD are not initialized in this phase.
- Wi-Fi setup button: GPIO18, active HIGH, external 4.7 kOhm pull-down, configured as `INPUT`.

All hardware identifiers and timing constants live in `HardwarePins.h`:

```cpp
#define TFT_CS       10
#define TFT_RST      14
#define TFT_DC        9
#define TFT_MOSI     11
#define TFT_SCLK     12
#define TFT_BL       13
#define Bt_SETWIFI   18
```

Additional fixed configuration:

- Button debounce: 30 ms.
- Long-press threshold: 2000 ms.
- TFT write clock: 40 MHz.
- Backlight PWM: 20 kHz, 8-bit duty resolution.
- Default brightness: 80 percent.
- Portal SSID: `SmartMediVend-Setup`.
- Portal password: `SmartMediVend`.
- Portal lifetime: 300000 ms. A timed-out portal returns to the underlying offline or reconnecting state and never reopens automatically.

The portal password is an onboarding default rather than a device secret. The README must tell deployers to replace it before production rollout.

## Reproducible Build

`SmartMediVend/sketch.yaml` defines the default Arduino CLI build profile:

- FQBN: `esp32:esp32:esp32s3`.
- Arduino-ESP32: 3.3.11.
- ESP32WiFiPortal: 2.1.2.
- LovyanGFX: 1.2.26.

The firmware compiles without PSRAM. PSRAM may be present, but correctness and the normal UI do not depend on it. Backlight code uses the Arduino-ESP32 3.x LEDC API and keeps a small compile-time compatibility branch for Arduino-ESP32 2.x.

## Source Layout

```text
SmartMediVend_ESP32S3/
├── SmartMediVend/
│   ├── SmartMediVend.ino
│   ├── HardwarePins.h
│   ├── RuntimeLogic.h
│   ├── WiFiManager.h
│   ├── WiFiManager.cpp
│   ├── DisplayManager.h
│   ├── DisplayManager.cpp
│   └── sketch.yaml
├── tests/
│   └── native/
│       └── RuntimeLogicTests.cpp
└── README.md
```

`RuntimeLogic.h` is the only additional production header. It contains small, Arduino-independent timing and button state machines shared by the firmware and native tests. It exists to make the most failure-prone time logic executable on the host without mocking ESP32 hardware.

## Execution Model

### Foreground Arduino task

The normal Arduino `setup()` initializes Serial, the display, and `WiFiManager`. The normal Arduino `loop()` performs only orchestration:

1. Call `wifiManager.update()` to copy the latest published Wi-Fi snapshot into foreground storage.
2. If its revision changed, map the application Wi-Fi state to a `DisplayManager` command.
3. Call `displayManager.update()` for time-based screen transitions and animation frames.

No Wi-Fi API, button wait, long animation, or unbounded operation is placed in the sketch.

### Wi-Fi worker

`WiFiManager` creates one long-lived FreeRTOS worker during `begin()` with an 8192-byte stack and normal application priority. The worker is the only application context that:

- calls any `ESP32WiFiPortal` method;
- calls `WiFi` accessors needed to publish SSID, IP, and RSSI;
- reads and debounces GPIO18;
- starts or stops the portal;
- invokes `portal.process()`.

This ownership rule satisfies the library's single-application-task constraint. The initial blocking `connectSaved(15000)` runs inside the worker, so the foreground display remains animated during the attempt. Once that call returns, the worker calls `portal.process()` at least once every 5 ms while yielding between iterations.

Callbacks registered with `ESP32WiFiPortal` execute in the worker context. They only set local event flags. They never draw, log credentials, allocate UI resources, or call display methods.

### Cross-task snapshot

The worker publishes a fixed-capacity `WiFiStatus` value under a short ESP32 critical section. It contains:

- `AppWiFiState state`;
- monotonically increasing `uint32_t revision`;
- SSID buffer of 33 bytes;
- portal SSID buffer of 33 bytes;
- error buffer of 96 bytes;
- local and portal IP addresses;
- RSSI;
- last disconnect reason;
- flags for connected, portal active, and portal connection attempt active.

All buffers are bounded and null-terminated. A revision changes only when consumer-visible state or data changes. `update()` copies the published value into foreground storage; display code never reads data while the worker is writing it.

## Runtime Timing Logic

Every deadline uses `uint32_t` and the wrap-safe comparison:

```cpp
static_cast<uint32_t>(now - startedAt) >= interval
```

`RuntimeLogic.h` owns the debounced active-HIGH long-press detector. Its behavior is:

1. A raw edge must remain stable for 30 ms before the debounced value changes.
2. The hold timer starts when the debounced value becomes HIGH.
3. The detector emits one event after 2000 ms of continuous debounced HIGH.
4. The emitted event latches until the debounced value becomes LOW.
5. A short press has no effect.
6. Holding for any additional duration cannot emit another event.
7. A debounced release rearms the next press.

There is no `delay(2000)` and no loop that waits for a pin or connection.

## Wi-Fi Manager

### Public interface

`WiFiManager` exposes initialization, foreground synchronization, and read-only state. It does not expose its `ESP32WiFiPortal` instance:

```cpp
class WiFiManager {
public:
    bool begin();
    bool update();
    const WiFiStatus& status() const;
};
```

`begin()` returns false only if the worker cannot be created. The foreground remains alive and the display presents a persistent fatal error in that case.

### Portal configuration

Before the first Wi-Fi operation, the worker performs:

```cpp
portal.setHostname("SmartMediVend");
portal.setConnectTimeout(15000);
portal.setConnectionRetryPolicy(3, 2000, 60000);
portal.setAutoReconnect(true);
```

All return values that can fail are checked. A configuration failure is published as `Error`; the device does not reboot repeatedly or continue with a partially configured policy.

The retry policy means one initial connection attempt plus at most three retries. Retry delay begins at 2000 ms and caps at 60000 ms. Further recovery is scheduled by the library's bounded cooldown policy. The application never calls `WiFi.begin()` and never implements a competing reconnect loop.

### Startup behavior

The application publishes `Connecting`, then calls `connectSaved(15000)` in the worker.

- Successful connection publishes `Connected` and a one-shot connection event.
- No saved credentials publishes `Offline`.
- Saved credentials that fail during the initial attempt publish `Offline`, while the library may continue its non-blocking saved-network recovery in the background.
- The portal does not start automatically after any boot failure.

### Portal behavior

A long-press event starts the portal only if `isPortalActive()` is false. The worker calls:

```cpp
portal.startConfigPortalAsync(
    "SmartMediVend-Setup",
    "SmartMediVend",
    300000);
```

State precedence while the portal is open is:

1. `PortalConnecting` when `isPortalConnectionAttemptActive()` is true.
2. `PortalActive` otherwise.

Submitting invalid credentials keeps the portal available for another attempt. The application publishes the library error as bounded display data but does not save the password, reboot, or close the portal early. A successful candidate generates the success transition and then `Connected`.

If portal startup fails, the UI shows a transient error for three seconds, then returns to the current `Offline` or `Reconnecting` condition. Releasing and holding the button again permits another start attempt.

### Reconnection behavior

After at least one successful STA connection, a loss of connection publishes `Reconnecting`. `ESP32WiFiPortal` alone manages retries and cooldowns through `process()`. A restored connection produces a one-shot success event and returns to `Connected`. Neither a disconnect nor exhaustion of a retry burst opens the portal.

## Application Wi-Fi States

```cpp
enum class AppWiFiState : uint8_t {
    Offline,
    Connecting,
    Connected,
    Reconnecting,
    PortalActive,
    PortalConnecting,
    Error
};
```

State changes are edge-driven. Publishing the same state and data does not increment the snapshot revision. RSSI is sampled no more than once per second and updates the snapshot only when its displayed signal-bar category changes or the numeric value changes by at least 3 dBm.

## Display Manager

### Driver

`DisplayManager.cpp` contains the project-local LovyanGFX device configuration:

- `lgfx::Panel_ILI9341`;
- portrait resolution 240 x 320;
- SPI2 host;
- MOSI GPIO11, SCLK GPIO12, MISO disabled;
- CS GPIO10, DC GPIO9, reset GPIO14;
- 40 MHz write clock;
- bus locking enabled;
- DMA enabled when supported by the selected Arduino-ESP32 core.

Touch and SD pins are not configured. The display configuration never changes global LovyanGFX or machine-installed library files.

### Public interface

`DisplayManager` owns the panel, backlight, layout, dirty-region tracking, and UI timers:

```cpp
class DisplayManager {
public:
    bool begin();
    void update();
    void showBoot();
    void applyWiFiStatus(const WiFiStatus& status);
    void setBrightness(uint8_t percent);
};
```

`DisplayManager` depends only on the application's `WiFiStatus` contract, not on `ESP32WiFiPortal` or direct Wi-Fi calls.

### Display states

```cpp
enum class DisplayState : uint8_t {
    Boot,
    WiFiConnecting,
    WiFiConnected,
    WiFiOffline,
    WiFiReconnecting,
    PortalActive,
    PortalConnecting,
    Success,
    Error,
    Home
};
```

Transitions use these fixed rules:

- `Boot` stays visible for at least 800 ms.
- Any disconnected-to-connected edge shows `Success` for 1500 ms, then `Home`.
- `Offline`, `PortalActive`, and persistent fatal `Error` remain until real state changes.
- A recoverable portal-start error stays visible for 3000 ms, then returns to the underlying Wi-Fi state.
- `Connecting`, `Reconnecting`, and `PortalConnecting` animate at 25 frames per second.

### Visual system

The screen uses a dark navy background, cyan primary accent, green success color, and amber/red warning colors. LovyanGFX primitives and built-in fonts draw all artwork; no filesystem or external bitmap is required.

Every screen uses three zones:

1. Header with `SmartMediVend` and a compact status badge.
2. Central 64-80 pixel icon or animation.
3. Lower content area for instructions or network details.

Required content:

- Boot: product name, `Smart Medicine Vending System`, and lightweight progress animation.
- Connecting: `Connecting to Wi-Fi...` with spinner.
- Connected/Success: check icon and `Wi-Fi Connected`.
- Offline: `Wi-Fi Offline` and instructions to hold the Wi-Fi button for two seconds.
- Reconnecting: disconnection message and animated reconnect status.
- Portal: AP SSID, setup password, portal IP, and phone connection instructions.
- Portal Connecting: selected-network connection message and spinner.
- Error: concise recoverable or fatal error without credentials.
- Home: `System Ready`, Wi-Fi badge, SSID, IP, and signal bars. It contains no placeholder controls for later product functions.

### Redraw and memory policy

- A static screen receives one full redraw on entry.
- Data fields redraw only when their rendered value changes.
- Only the animation area uses a 16-bit sprite, allocated once during `begin()`.
- The sprite is at most 80 x 80 pixels, approximately 12.8 KiB.
- If sprite allocation fails, animation falls back to clearing and redrawing the same bounded rectangle directly.
- No full-screen framebuffer is allocated.
- The render loop creates no temporary `String` objects and performs no periodic allocation/free cycle.
- `update()` returns immediately when no transition or frame is due.

### Backlight

GPIO13 is configured once during display initialization. `setBrightness()` clamps input to 0-100, converts it to the configured 8-bit duty, and writes the PWM only when duty changes. LEDC is never reattached during normal operation.

## Error Handling and Logging

Serial logging uses the prefixes `[BOOT]`, `[WIFI]`, `[PORTAL]`, `[DISPLAY]`, and `[BUTTON]`. Logs are emitted on state transitions and failures, not every loop. Wi-Fi passwords and submitted portal passwords are never logged or copied into application snapshots.

Failure policy:

- Display initialization failure: log once; keep the Wi-Fi manager operational with the backlight off.
- Wi-Fi worker creation failure: keep the foreground loop alive and report a persistent display error if the display is available.
- Invalid portal library configuration: publish a fatal Wi-Fi error and do not begin partially configured networking.
- Portal start failure: publish a recoverable error, retain existing STA behavior, and allow a later button attempt.
- Invalid selected credentials: retain the portal and display a retry message.
- Router outage: show reconnecting while library-managed bounded recovery continues.

No error path uses an automatic reboot as recovery.

## Testing Strategy

### Native deterministic tests

`tests/native/RuntimeLogicTests.cpp` builds with the installed Microsoft C++ toolchain and tests the actual production timing code. Each test uses literal event times and expected events. Coverage includes:

- a press shorter than 2000 ms does not emit;
- 2000 ms of debounced HIGH emits once;
- a continuous 10-second hold still emits once;
- a debounced release rearms a second long press;
- raw bounce shorter than 30 ms does not alter the debounced state;
- bounce during a hold cannot create an early or duplicate event;
- hold and display deadlines work when timestamps cross `UINT32_MAX`;
- portal-active and portal-connecting signals have correct state precedence;
- unchanged consumer-visible values do not require a new revision.

### Firmware build verification

Arduino CLI compiles the actual `SmartMediVend` sketch using the pinned `esp32s3` profile. Successful verification requires exit code zero and a reported flash/RAM footprint that fits the generic ESP32-S3 target without PSRAM.

### Hardware acceptance checklist

The README documents tests that require the physical ESP32-S3, TFT, button, phone, and router:

- short press, exact long press, continuous hold, release/repress, and button bounce;
- saved valid credentials, no credentials, router outage/recovery, active-portal duplicate prevention, valid and invalid candidate credentials;
- correct ILI9341 colors/orientation, smooth animation, no visible static-screen flicker, responsive portal during animation;
- free heap before and after repeated portal sessions, reconnect cycles, and extended operation;
- no watchdog reset, stack overflow, reconnect storm, or downward heap trend.

The final report distinguishes automated results from items that remain pending on physical hardware.

## README Deliverable

The root README is rewritten in Vietnamese and includes:

- project purpose and current phase boundaries;
- source tree and module responsibilities;
- complete GPIO table and wiring notes;
- pinned dependencies;
- Arduino CLI and Arduino IDE build instructions;
- portal SSID/password, button procedure, timeout, and security note;
- all TFT states and expected transitions;
- automated and hardware test procedures;
- troubleshooting for blank TFT, wrong rotation/colors, portal access, and serial logs;
- future extension points for AudioManager, XiaozhiManager, ConversationManager, MedicationSafetyEngine, InventoryManager, VendingController, and PaymentManager without implementing them.

## Acceptance Criteria

The implementation is complete only when:

1. The native timing/state tests pass.
2. The pinned Arduino CLI profile compiles for generic ESP32-S3 with exit code zero.
3. All GPIO values match the confirmed wiring.
4. GPIO18 is active HIGH with software debounce and one-shot 2000 ms long-press behavior.
5. Only the Wi-Fi worker calls `ESP32WiFiPortal`, and it services `process()` frequently after startup.
6. Portal startup is asynchronous, button-gated, duplicate-safe, and never automatic after Wi-Fi failure.
7. Auto Reconnect uses the library policy without a competing loop.
8. TFT rendering is non-blocking, state-driven, and allocation-stable during runtime.
9. `SmartMediVend.ino` contains orchestration only.
10. The Vietnamese README documents build, operation, testing, and hardware-only verification gaps.
11. No out-of-scope audio, Xiaozhi, medical, payment, inventory, or vending behavior is added.

