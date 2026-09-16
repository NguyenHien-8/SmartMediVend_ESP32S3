# SmartMediVend Wi-Fi Portal and TFT Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Deliver a buildable ESP32-S3 firmware that manages saved Wi-Fi and an on-demand asynchronous portal while rendering a non-blocking ILI9341 UI.

**Architecture:** One FreeRTOS worker exclusively owns ESP32WiFiPortal, Wi-Fi access, and the GPIO18 button, then publishes bounded snapshots to the Arduino foreground task. The foreground task feeds a state-driven LovyanGFX display manager whose static screens redraw only on changes and whose animation uses one small reusable sprite.

**Tech Stack:** C++17, Arduino-ESP32 3.3.11, ESP32WiFiPortal 2.1.2, LovyanGFX 1.2.26, Arduino CLI build profiles, MSVC native tests.

**Spec:** `docs/superpowers/specs/2026-09-17-smartmedivend-wifi-tft-design.md`

## Global Constraints

- Target FQBN is `esp32:esp32:esp32s3`; firmware must compile without PSRAM.
- Pin assignments are TFT CS 10, reset 14, DC 9, MOSI 11, SCLK 12, backlight 13, and active-HIGH Wi-Fi button 18.
- GPIO18 uses `INPUT` with an external 4.7 kOhm pull-down, 30 ms debounce, and one event per continuous 2000 ms hold.
- Portal SSID/password are `SmartMediVend-Setup` / `SmartMediVend`, with a 300000 ms timeout.
- Portal calls stay in one worker task; the application never calls `WiFi.begin()` and never creates a competing reconnect policy.
- `portal.process()` is serviced at least once per 5 ms worker iteration after startup.
- Runtime timers use unsigned subtraction and remain correct across `millis()` wrap.
- Runtime render and Wi-Fi loops perform no periodic allocation/free cycle and log no Wi-Fi password.
- Touch, SD, Xiaozhi, audio, diagnosis, medication, payment, inventory, and vending behavior remain out of scope.

---

### Task 1: Deterministic runtime logic and hardware constants

**Files:**
- Create: `SmartMediVend/HardwarePins.h`
- Create: `SmartMediVend/RuntimeLogic.h`
- Create: `tests/native/RuntimeLogicTests.cpp`

**Interfaces:**
- Consumes: standard `<cstdint>` and boolean raw button samples.
- Produces: `AppWiFiState`, `RuntimeSignals`, `deriveWiFiState(const RuntimeSignals&)`, `hasElapsed(uint32_t, uint32_t, uint32_t)`, and `LongPressDetector::update(bool, uint32_t)`.

- [ ] **Step 1: Add native tests that name each timing and state failure**

Create a self-contained test executable with literal timestamps and a small `expect()` helper. The tests must call production declarations from `SmartMediVend/RuntimeLogic.h` and cover:

```cpp
expect(!button.update(true, 0), "raw press must debounce");
expect(!button.update(true, 29), "29 ms is not debounced");
expect(!button.update(true, 30), "debounce edge is not a hold");
expect(!button.update(true, 2029), "1999 ms stable hold is short");
expect(button.update(true, 2030), "2000 ms stable hold emits");
expect(!button.update(true, 10030), "continuous hold emits only once");
```

Add independent cases for stable release/repress, sub-30-ms bounce, bounce during the hold, a hold beginning at `UINT32_MAX - 1000`, state precedence (`Error`, `PortalConnecting`, `PortalActive`, `Connected`, `Connecting`, `Reconnecting`, `Offline`), and `hasElapsed()` across wrap.

- [ ] **Step 2: Run the native test and verify RED**

Run from a Visual Studio developer environment:

```powershell
New-Item -ItemType Directory -Force build\native
cl /nologo /std:c++17 /EHsc /I SmartMediVend tests\native\RuntimeLogicTests.cpp /Fe:build\native\RuntimeLogicTests.exe
```

Expected: compilation fails because `RuntimeLogic.h` and its API do not exist.

- [ ] **Step 3: Implement the minimal wrap-safe state machines and pin constants**

`HardwarePins.h` defines the seven required GPIO macros plus these typed constants:

```cpp
constexpr uint32_t BUTTON_DEBOUNCE_MS = 30;
constexpr uint32_t BUTTON_HOLD_MS = 2000;
constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 15000;
constexpr uint32_t WIFI_PORTAL_TIMEOUT_MS = 300000;
constexpr uint32_t WIFI_WORKER_INTERVAL_MS = 5;
constexpr uint8_t DEFAULT_BRIGHTNESS_PERCENT = 80;
```

`RuntimeLogic.h` implements `hasElapsed()` with unsigned subtraction, the seven-value `AppWiFiState`, `RuntimeSignals`, and this precedence:

```text
fatalError > portalConnecting > portalActive > connected >
startupConnecting > everConnected/Reconnecting > Offline
```

`LongPressDetector` stores raw state, debounced state, raw-edge timestamp, stable-press timestamp, and a one-shot latch. A stable LOW clears the latch and rearms the detector.

- [ ] **Step 4: Run the native test and verify GREEN**

Run the compile command from Step 2, then:

```powershell
build\native\RuntimeLogicTests.exe
```

Expected: exit code 0 and a final line `RuntimeLogicTests: all checks passed`.

- [ ] **Step 5: Commit runtime logic**

```powershell
git add SmartMediVend/HardwarePins.h SmartMediVend/RuntimeLogic.h tests/native/RuntimeLogicTests.cpp
git commit -m "test: define wrap-safe runtime logic"
```

---

### Task 2: Single-owner Wi-Fi worker and snapshot contract

**Files:**
- Create: `SmartMediVend/WiFiManager.h`
- Create: `SmartMediVend/WiFiManager.cpp`
- Modify: `tests/native/RuntimeLogicTests.cpp`

**Interfaces:**
- Consumes: `LongPressDetector`, `RuntimeSignals`, ESP32WiFiPortal 2.1.2, Arduino `WiFi`, GPIO18, and FreeRTOS task/critical-section APIs.
- Produces: `WiFiStatus`, `WiFiManager::begin()`, `WiFiManager::update()`, and `WiFiManager::status()`.

- [ ] **Step 1: Extend native tests for publish-decision behavior**

Add a production helper in the desired API and tests proving consumer-visible equality ignores `revision` but detects state, SSID, IP octets, RSSI display threshold, portal flags, and error changes. Use fixed `StatusView` data from `RuntimeLogic.h`, not a duplicate test model:

```cpp
StatusView a{};
StatusView b{};
a.state = b.state = AppWiFiState::Offline;
expect(sameVisibleStatus(a, b), "identical status must not republish");
b.portalActive = true;
expect(!sameVisibleStatus(a, b), "portal change must republish");
```

- [ ] **Step 2: Run native tests and verify RED**

Run the Task 1 compile and executable commands.

Expected: compilation fails because `StatusView` and `sameVisibleStatus()` do not exist.

- [ ] **Step 3: Add the fixed-capacity status contract**

Extend `RuntimeLogic.h` with an Arduino-independent `StatusView` containing state, `char ssid[33]`, `char portalSsid[33]`, `char error[96]`, four-byte local/portal IP arrays, RSSI, disconnect reason, and three portal/connection flags. Implement field-by-field `sameVisibleStatus()` and `rssiNeedsPublish(previous, current)` with a 3 dBm threshold or signal-bar category change.

`WiFiManager.h` wraps `StatusView` in:

```cpp
struct WiFiStatus : StatusView {
    uint32_t revision = 0;
};

class WiFiManager {
public:
    bool begin();
    bool update();
    const WiFiStatus& status() const;
private:
    static void taskEntry(void* context);
    void run();
    void refreshStatus(uint32_t now, bool force);
    void publish(const WiFiStatus& candidate);
};
```

Keep the portal private and keep all status buffers fixed-capacity.

- [ ] **Step 4: Implement the worker ownership and portal lifecycle**

`WiFiManager::begin()` configures GPIO18 as `INPUT`, seeds an `Offline` foreground/published snapshot, then starts one 8192-byte FreeRTOS task. The task:

1. Registers callbacks that set worker-local flags only.
2. Validates `setHostname("SmartMediVend")` and `setConnectionRetryPolicy(3, 2000, 60000)`.
3. Applies the 15000 ms timeout and enables library Auto Reconnect.
4. Publishes `Connecting` and runs `connectSaved(15000)`.
5. Publishes initial success or offline state without opening a portal.
6. In the lifetime loop, calls `portal.process()`, samples the long-press detector, starts one async portal when allowed, refreshes changed state/data, and calls `vTaskDelay(pdMS_TO_TICKS(5))`.

Use `isPortalConnectionAttemptActive()`, `isPortalActive()`, `isConnected()`, and tracked `everConnected/startupConnecting/fatalError` to construct `RuntimeSignals`. Cache library `String` results only on connection/portal/error transitions, copy with bounded `snprintf`, and sample RSSI no faster than once per second.

`publish()` increments revision only when `sameVisibleStatus()` reports a consumer-visible difference, then copies under `portENTER_CRITICAL`. `update()` copies the published snapshot under the same mux and returns true only for a new revision.

- [ ] **Step 5: Run native tests and compile the Wi-Fi translation unit through the firmware build later**

Run the Task 1 native test commands.

Expected: all native checks pass. Arduino-specific compilation is deferred to Task 4, where the exact pinned libraries are present.

- [ ] **Step 6: Commit Wi-Fi management**

```powershell
git add SmartMediVend/RuntimeLogic.h SmartMediVend/WiFiManager.h SmartMediVend/WiFiManager.cpp tests/native/RuntimeLogicTests.cpp
git commit -m "feat: add single-owner Wi-Fi portal manager"
```

---

### Task 3: ILI9341 display manager with bounded redraw

**Files:**
- Create: `SmartMediVend/DisplayManager.h`
- Create: `SmartMediVend/DisplayManager.cpp`

**Interfaces:**
- Consumes: `WiFiStatus`, LovyanGFX 1.2.26, TFT GPIO constants, and wrap-safe timing helpers.
- Produces: `DisplayState`, `DisplayManager::begin()`, `showBoot()`, `applyWiFiStatus()`, `update()`, and `setBrightness()`.

- [ ] **Step 1: Define the display contract before driver code**

Create `DisplayManager.h` with the ten design states and exact public methods:

```cpp
enum class DisplayState : uint8_t {
    Boot, WiFiConnecting, WiFiConnected, WiFiOffline,
    WiFiReconnecting, PortalActive, PortalConnecting,
    Success, Error, Home
};

class DisplayManager {
public:
    bool begin();
    void update();
    void showBoot();
    void applyWiFiStatus(const WiFiStatus& status);
    void setBrightness(uint8_t percent);
};
```

Private state stores the current display state, state-entry time, next animation time, last rendered status, sprite availability, spinner frame, current duty, and recoverable-error return state.

- [ ] **Step 2: Implement the project-local LovyanGFX device**

In `DisplayManager.cpp`, define one module-private `lgfx::LGFX_Device` subclass with `lgfx::Bus_SPI` and `lgfx::Panel_ILI9341`. Configure SPI2, mode 0, 40 MHz write, MISO `-1`, GPIO11/12/9 bus pins, GPIO10/14 panel pins, 240 x 320 dimensions, bus locking, and automatic DMA. Do not modify LovyanGFX's installed headers.

Initialize the panel in portrait rotation, set 16-bit color depth, clear once, and allocate one 80 x 80 16-bit sprite. If sprite allocation returns null, retain a direct-draw animation path over the same rectangle.

- [ ] **Step 3: Implement stable backlight control**

For Arduino-ESP32 3.x, attach GPIO13 once with `ledcAttach(TFT_BL, 20000, 8)` and write duty by pin. For 2.x, use one fixed LEDC channel through `ledcSetup`, `ledcAttachPin`, and channel-based `ledcWrite`. Clamp percent to 0-100 and skip a write when duty is unchanged.

- [ ] **Step 4: Implement state entry screens and partial animation**

Use a navy/cyan/green/amber palette and built-in fonts. Each state entry clears once and draws header, icon frame, and text fields. Implement the required copy exactly in English as approved by the spec, including AP SSID/password/IP on the portal screen and SSID/IP/RSSI bars on connected/home screens.

`update()` uses 40 ms frame spacing for connecting states, 800 ms minimum boot duration, 1500 ms success duration, and 3000 ms recoverable-error duration. It redraws only the 80 x 80 animation area for spinner frames and returns immediately when no frame or transition is due.

`applyWiFiStatus()` maps application states, detects disconnected-to-connected edges for `Success`, retains persistent fatal errors, and redraws data fields only when their rendered values differ.

- [ ] **Step 5: Commit the display manager**

```powershell
git add SmartMediVend/DisplayManager.h SmartMediVend/DisplayManager.cpp
git commit -m "feat: add non-blocking ILI9341 interface"
```

---

### Task 4: Foreground orchestration and reproducible ESP32-S3 build

**Files:**
- Modify: `SmartMediVend/SmartMediVend.ino`
- Create: `SmartMediVend/sketch.yaml`

**Interfaces:**
- Consumes: `WiFiManager` and `DisplayManager` public APIs.
- Produces: the complete Arduino sketch and pinned build profile.

- [ ] **Step 1: Replace the empty sketch with orchestration only**

Implement this control shape without Wi-Fi or rendering algorithms in the sketch:

```cpp
DisplayManager displayManager;
WiFiManager wifiManager;

void setup() {
    Serial.begin(115200);
    const bool displayReady = displayManager.begin();
    if (displayReady) displayManager.showBoot();
    if (!wifiManager.begin() && displayReady) {
        displayManager.applyWiFiStatus(wifiManager.status());
    }
}

void loop() {
    if (wifiManager.update()) {
        displayManager.applyWiFiStatus(wifiManager.status());
    }
    displayManager.update();
    taskYIELD();
}
```

- [ ] **Step 2: Add the pinned Arduino CLI profile**

Create `sketch.yaml` with profile `esp32s3`, FQBN `esp32:esp32:esp32s3`, Arduino-ESP32 3.3.11 and its stable package index URL, ESP32WiFiPortal 2.1.2, LovyanGFX 1.2.26, and `default_profile: esp32s3`.

- [ ] **Step 3: Install an isolated Arduino CLI and build through the profile**

Download the official Windows Arduino CLI into an untracked workspace tool directory, initialize its local config/data directories, and run:

```powershell
arduino-cli compile --profile esp32s3 SmartMediVend
```

Expected: exit code 0 with flash and static RAM usage printed for generic ESP32-S3. If profile installation reports an unavailable version, query the official index, correct only an objectively invalid dependency identifier, and retain the approved version numbers.

- [ ] **Step 4: Fix compile failures without changing the approved behavior**

For every compiler error, make the smallest source/API correction and repeat the full profile compile until exit code 0. Do not replace ESP32WiFiPortal, ILI9341, or the single-worker design to avoid an error.

- [ ] **Step 5: Re-run native tests**

Build and execute `RuntimeLogicTests.exe` using the Task 1 commands.

Expected: all checks pass after integration changes.

- [ ] **Step 6: Commit the integrated firmware and profile**

```powershell
git add SmartMediVend
git commit -m "feat: integrate SmartMediVend Wi-Fi and TFT runtime"
```

---

### Task 5: Vietnamese operations guide and full verification

**Files:**
- Modify: `README.md`

**Interfaces:**
- Consumes: final source behavior, build commands, and observed verification output.
- Produces: Vietnamese setup, operation, testing, troubleshooting, and expansion documentation.

- [ ] **Step 1: Rewrite README in Vietnamese**

Document the phase boundary, tree, module ownership, GPIO table, wiring, exact dependency versions, Arduino CLI profile command, Arduino IDE setup, portal button procedure, AP credentials and timeout, every TFT state, expected serial prefixes, and blank-screen/rotation/color/portal troubleshooting.

Include separate sections for automated checks and a physical-hardware checklist. State explicitly that touch and microSD are unused and that hardware-only observations are not claimed by desktop tests.

- [ ] **Step 2: Run fresh native verification**

Compile and run `RuntimeLogicTests.exe` again.

Expected: exit code 0 and `RuntimeLogicTests: all checks passed`.

- [ ] **Step 3: Run fresh firmware verification**

Run:

```powershell
arduino-cli compile --profile esp32s3 SmartMediVend
```

Expected: exit code 0 and memory usage below target limits.

- [ ] **Step 4: Inspect repository changes and requirement coverage**

Run `git diff --check`, inspect `git status --short`, and compare every acceptance criterion in the design spec against a source file, native test, build result, README section, or explicitly pending physical-hardware test.

- [ ] **Step 5: Commit documentation**

```powershell
git add README.md
git commit -m "docs: add SmartMediVend setup and test guide"
```

- [ ] **Step 6: Report evidence and hardware gaps**

The final report lists created/modified files, exact dependency versions, native test output, Arduino compile result and memory footprint, architecture summary, and the physical TFT/Wi-Fi/heap/watchdog checks that still require the user's connected hardware.
