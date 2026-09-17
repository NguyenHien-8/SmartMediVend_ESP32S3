# SmartMediVend ESP32-S3 End-to-End Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a self-contained Arduino IDE firmware for ESP32-S3-N16R8 that provides Xiaozhi voice conversation, deterministic local medical screening, protected inventory, explicit confirmation, and safe sequential 500 ms active-LOW vending.

**Architecture:** Keep hardware and cloud adapters behind small interfaces while the medical, inventory, confirmation, protocol, and vending cores remain portable C++ that can be host-tested. Treat AI/cloud input as untrusted structured observations; only the local rule engine may create candidates and only `VendGuard` may authorize `VendingManager`. Use one centralized event-driven application state machine and bounded queues/buffers.

**Tech Stack:** Arduino-ESP32 stable 3.x, C++17, ESP-IDF APIs shipped inside Arduino-ESP32, ESP32WiFiPortal 2.1.2, ArduinoJson 7.x, Adafruit GFX/ST7789/BusIO, arduinoWebSockets, Xiph Opus fixed-point, CMake/CTest host tests.

**Spec:** `docs/superpowers/specs/2026-09-17-smartmedivend-esp32s3-design.md`

## Global Constraints

- Edit the current `main` checkout directly; do not create a branch, copy, or worktree.
- Target ESP32-S3-WROOM-1-N16R8 with 16 MB flash and 8 MB Octal PSRAM.
- GPIO mapping is fixed by the spec; `MUX_SIG=GPIO17`, and GPIO35–37 are unavailable.
- GPIO43/44 and all medicine-drop sensors are unused.
- Relay output is active LOW for 500 ms; one relay at a time; stock decrements after the completed pulse as `COMMAND_SENT_UNVERIFIED`.
- GPIO18 short press toggles listening; hold for 2 seconds opens Wi-Fi Portal and suppresses short press.
- Users under 16, pregnancy/breastfeeding, red flags, contraindications, conflicts, and missing data must fail closed.
- AI/MCP/network code may not choose a SKU/channel or directly start vending.
- `PRODUCTION_VENDING_ENABLED=false` and pharmacist approval is required by default.
- No real token, Wi-Fi credential, or insecure TLS default may enter source control.
- Use TDD for new behavior: test, observe the expected failure, implement minimally, rerun, then refactor.
- Every task ends with a focused commit after its stated tests pass.

## File Map

The implementation will create or modify these responsibility boundaries:

```text
SmartMediVend/
├── SmartMediVend.ino                  composition root only
├── AppConfig.h                        compile-time non-secret configuration
├── HardwarePins.h                     final GPIO map and conflict assertions
├── data/
│   ├── medicines.json                 16 physical channels and source metadata
│   ├── medical_rules.json             pharmacist-reviewable deterministic rules
│   └── pharmacist_review.json         production lock artifact
└── src/
    ├── app/                            app state, events, controller, button
    ├── core/                           portable time/result/string helpers
    ├── medical/                        input validation, policy, catalog, rules
    ├── inventory/                      stock, backup routing, persistence model
    ├── vending/                        confirmation, guard, sequencer, relay adapter
    ├── protocol/                       Xiaozhi JSON/binary codecs
    ├── mcp/                            bounded JSON-RPC dispatcher and safe tools
    ├── network/                        Wi-Fi owner, bootstrap, WSS transport/session
    ├── audio/                          I2S RX/TX, Opus, fixed queues
    ├── ui/                             ST7789 renderer and view models
    ├── storage/                        NVS settings/inventory journal
    ├── diagnostics/                    health counters/log snapshots
    └── vendor/                         pinned dependency source + licenses
tests/host/                              portable unit/integration test executable
tools/                                  deterministic validation/build scripts
```

---

### Task 1: Establish host test harness, final pins, and core state types

**Files:**
- Create: `tests/host/CMakeLists.txt`
- Create: `tests/host/TestHarness.h`
- Create: `tests/host/test_main.cpp`
- Create: `tests/host/test_app_types.cpp`
- Create: `SmartMediVend/src/app/AppState.h`
- Create: `SmartMediVend/src/app/AppEvent.h`
- Create: `SmartMediVend/src/core/Elapsed.h`
- Modify: `SmartMediVend/HardwarePins.h`
- Modify: `SmartMediVend/AppConfig.h`

**Interfaces:**
- Produces: `enum class AppState`, `enum class AppEventType`, `bool elapsedMs(uint32_t now, uint32_t since, uint32_t interval)`.
- Produces final `smv::pins::*` constants used by every hardware adapter.

- [ ] **Step 1: Add a minimal test runner and failing state/pin tests**

```cpp
TEST_CASE(app_starts_in_booting_and_wrap_safe_elapsed_works) {
  REQUIRE(static_cast<uint8_t>(smv::AppState::Booting) == 0);
  REQUIRE(smv::elapsedMs(4U, 0xFFFFFFF0U, 20U));
}

TEST_CASE(final_hardware_map_has_no_conflicts) {
  constexpr std::array<uint8_t, 20> pins = {10,14,9,11,12,13,18,6,4,5,
                                             16,15,7,39,40,41,42,17,43,44};
  REQUIRE(allUniqueExceptUnused43And44(pins));
  REQUIRE(smv::pins::MUX_SIG == 17);
}
```

- [ ] **Step 2: Configure and run the host target to observe missing-type failures**

Run: `cmake -S tests/host -B build/host && cmake --build build/host && ctest --test-dir build/host --output-on-failure`

Expected: compilation fails because `AppState`, `elapsedMs`, and the final MUX pins do not exist.

- [ ] **Step 3: Add the minimal portable types and complete GPIO assertions**

```cpp
enum class AppState : uint8_t {
  Booting = 0, WifiConnecting, WifiPortal, Offline, CloudConnecting,
  Idle, Listening, Processing, Speaking, AwaitingConfirmation,
  Dispensing, Recovering, Error
};

constexpr bool elapsedMs(uint32_t now, uint32_t since, uint32_t interval) {
  return static_cast<uint32_t>(now - since) >= interval;
}
```

Add INMP441, MAX98357A, S0–S3, SIG=17, and reserved GPIO43/44 constants. Use one constexpr array plus a constexpr uniqueness function in `HardwarePins.h` so adding any duplicate assigned GPIO fails compilation.

- [ ] **Step 4: Run host tests and `git diff --check`**

Expected: all Task 1 tests pass and whitespace check exits 0.

- [ ] **Step 5: Commit**

```bash
git add SmartMediVend/HardwarePins.h SmartMediVend/AppConfig.h SmartMediVend/src tests/host
git commit -m "test: establish portable firmware core"
```

---

### Task 2: Add the 16-channel catalog and pharmacist production gate

**Files:**
- Create: `SmartMediVend/data/medicines.json`
- Create: `SmartMediVend/data/medical_rules.json`
- Create: `SmartMediVend/data/pharmacist_review.json`
- Create: `SmartMediVend/src/medical/MedicineCatalog.h`
- Create: `SmartMediVend/src/medical/MedicineCatalog.cpp`
- Create: `SmartMediVend/src/medical/PharmacistGate.h`
- Create: `SmartMediVend/src/medical/PharmacistGate.cpp`
- Create: `tests/host/test_catalog.cpp`
- Create: `tools/validate_medical_json.ps1`

**Interfaces:**
- Produces: `Medicine`, `PhysicalSlot`, `MedicineCatalog::findBySku()`, `MedicineCatalog::resolveAvailableSlot()`, and `PharmacistGate::allowsProductionVending()`.
- JSON artifacts use `schema_version`, `catalog_version`, `rules_version`, `reviewed_catalog_version`, `reviewed_rules_version`, and `approved`.

- [ ] **Step 1: Write failing catalog and gate tests**

```cpp
TEST_CASE(catalog_has_16_slots_13_unique_medicines_and_exact_backups) {
  const auto catalog = MedicineCatalog::builtIn();
  REQUIRE(catalog.slotCount() == 16);
  REQUIRE(catalog.uniqueMedicineCount() == 13);
  REQUIRE(catalog.slot(13).backupOfChannel == 0);
  REQUIRE(catalog.slot(14).backupOfChannel == 3);
  REQUIRE(catalog.slot(15).backupOfChannel == 7);
}

TEST_CASE(unapproved_or_version_mismatched_review_blocks_production) {
  REQUIRE_FALSE(PharmacistGate::allowsProductionVending(
      Review{false, "catalog-2026-09-17", "rules-2026-09-17"},
      "catalog-2026-09-17", "rules-2026-09-17"));
  REQUIRE_FALSE(PharmacistGate::allowsProductionVending(
      Review{true, "wrong", "rules-2026-09-17"},
      "catalog-2026-09-17", "rules-2026-09-17"));
}
```

- [ ] **Step 2: Run the focused test and confirm missing catalog/gate failures**

Run: `cmake --build build/host && build/host/Debug/smv_host_tests.exe catalog`

Expected: compilation fails on missing `MedicineCatalog` and `PharmacistGate`.

- [ ] **Step 3: Implement the immutable catalog and default-deny review gate**

Use fixed-capacity `std::array<PhysicalSlot, 16>` and `std::array<Medicine, 13>`. Store SKU, display name, active ingredient, strength text, symptom domain, main channel, optional backup channel, and initial stock. No catalog API accepts a relay/channel value from external JSON at runtime.

Create valid JSON containing exactly the table in the approved spec. `pharmacist_review.json` must contain:

```json
{
  "schema_version": 1,
  "approved": false,
  "reviewed_catalog_version": null,
  "reviewed_rules_version": null,
  "reviewer": null,
  "reviewed_at": null,
  "notice": "PHARMACIST_REVIEW_REQUIRED"
}
```

- [ ] **Step 4: Validate JSON and run catalog tests**

Run: `powershell -ExecutionPolicy Bypass -File tools/validate_medical_json.ps1`

The script must parse all three files with `ConvertFrom-Json`, assert 16 unique channels 0–15, stock=5, exact backup mapping, 13 unique canonical medicine IDs, and `approved=false`.

Run: `ctest --test-dir build/host --output-on-failure`

- [ ] **Step 5: Commit**

```bash
git add SmartMediVend/data SmartMediVend/src/medical tests/host/test_catalog.cpp tools/validate_medical_json.ps1
git commit -m "feat: add reviewed medicine catalog gate"
```

---

### Task 3: Validate untrusted symptom data and enforce global safety policy

**Files:**
- Create: `SmartMediVend/src/medical/SymptomSession.h`
- Create: `SmartMediVend/src/medical/StructuredInputValidator.h`
- Create: `SmartMediVend/src/medical/StructuredInputValidator.cpp`
- Create: `SmartMediVend/src/medical/SafetyPolicy.h`
- Create: `SmartMediVend/src/medical/SafetyPolicy.cpp`
- Create: `tests/host/test_input_safety.cpp`

**Interfaces:**
- Consumes: fixed enums only; it never consumes SKU/channel.
- Produces: `ValidationResult`, `SafetyDecision { NeedMoreInfo, Deny, Refer, Offer }`, and a bounded `SymptomSession`.

- [ ] **Step 1: Write table-driven failing tests for missing/unsafe data**

```cpp
TEST_CASE(global_policy_fails_closed) {
  const Case cases[] = {
    {validSession().withAge(15), SafetyDecision::Deny},
    {validSession().withAgeUnknown(), SafetyDecision::NeedMoreInfo},
    {validSession().withPregnancy(true), SafetyDecision::Deny},
    {validSession().withDanger(DangerSign::DifficultyBreathing), SafetyDecision::Deny},
    {validSession().withWeightUnknown(), SafetyDecision::NeedMoreInfo},
    {validSession().withMedicinesUnknown(), SafetyDecision::NeedMoreInfo},
  };
  for (const auto& c : cases) REQUIRE(SafetyPolicy{}.evaluate(c.input).decision == c.want);
}

TEST_CASE(ai_fields_that_attempt_vending_are_rejected) {
  const char json[] = R"({"session_id":"s","turn_id":1,"age_years":20,
                           "sku":"SMV-PARA500","relay":0,"vend":true})";
  REQUIRE(StructuredInputValidator{}.validate(json).error ==
          ValidationError::ForbiddenAuthorityField);
}
```

- [ ] **Step 2: Run tests and observe missing validator/policy failures**

Run the host test executable filtered to `safety`.

- [ ] **Step 3: Implement bounded session enums and fail-closed evaluation**

Use fixed arrays with explicit counts: at most 8 symptoms, 16 current medicine groups, 12 conditions, 8 allergies, and 16 danger signs. Accept age 16–120 and weight 25–300 kg as structurally plausible; per-medicine weight policy remains in Task 4. Reject unknown keys that imply authority (`sku`, `channel`, `relay`, `quantity`, `vend`) and cap JSON input at 4096 bytes.

- [ ] **Step 4: Run all host tests and mutation-check one branch**

Temporarily change the `<16` check to `<15`, run the age test and observe failure, restore, then rerun all tests.

- [ ] **Step 5: Commit**

```bash
git add SmartMediVend/src/medical tests/host/test_input_safety.cpp
git commit -m "feat: enforce fail-closed symptom safety"
```

---

### Task 4: Implement all 13 deterministic medicine rules and conflict matrix

**Files:**
- Create: `SmartMediVend/src/medical/MedicalRuleEngine.h`
- Create: `SmartMediVend/src/medical/MedicalRuleEngine.cpp`
- Create: `SmartMediVend/src/medical/MedicineRules.h`
- Create: `SmartMediVend/src/medical/MedicineRules.cpp`
- Create: `tests/host/test_medical_rules.cpp`
- Modify: `SmartMediVend/data/medical_rules.json`

**Interfaces:**
- Consumes: a globally safe `SymptomSession` and `MedicineCatalog`.
- Produces: `CandidateResult` containing decision, reason codes, and at most three canonical medicine IDs; never a channel.

- [ ] **Step 1: Write one failing positive and negative test per medicine**

Use literal fixtures for all 13 medicines. Representative cases:

```cpp
TEST_CASE(dry_cough_selects_dextromethorphan_but_productive_cough_does_not) {
  REQUIRE(evaluate(validSession().withSymptom(Symptom::DryCough)).has("SMV-DXM15"));
  REQUIRE_FALSE(evaluate(validSession().withSymptom(Symptom::ProductiveCough)).has("SMV-DXM15"));
}

TEST_CASE(omeprazole_and_bisacodyl_refer_age_16_or_17) {
  REQUIRE(evaluate(validSession().withAge(17).withSymptom(Symptom::AcidReflux)).decision ==
          SafetyDecision::Refer);
  REQUIRE(evaluate(validSession().withAge(16).withSymptom(Symptom::Constipation)).decision ==
          SafetyDecision::Refer);
}

TEST_CASE(paracetamol_refers_below_50kg_and_rejects_duplicate_ingredient) {
  REQUIRE(evaluate(validSession().withWeight(49).withSymptom(Symptom::MildHeadache)).decision ==
          SafetyDecision::Refer);
  REQUIRE_FALSE(evaluate(validSession().withMedicine(MedicineGroup::ContainsParacetamol)
                                       .withSymptom(Symptom::MildHeadache)).offered());
}
```

Add negative fixtures for all exclusions in spec section 8 and conflict tests for dextromethorphan+ambroxol, antacid+omeprazole, duplicate antihistamines, sedating combinations, duplicate canonical medicines, and more than three domains.

- [ ] **Step 2: Run the focused rule tests and confirm missing engine failures**

Expected: compilation fails because `MedicalRuleEngine` is absent.

- [ ] **Step 3: Implement one rule table and deterministic ranking**

Evaluate global policy first, then medicine-specific predicates. Stable priority is symptom-domain order from the session, then catalog order. Return `Refer` when a known condition requires a pharmacist; return `Deny` for red flags/absolute exclusions; return `NeedMoreInfo` for unknown required fields. Limit offers to one medicine per domain and three total.

- [ ] **Step 4: Run rules, JSON validation, and full host suite**

Expected: every positive, exclusion, interaction, age, weight, and limit test passes.

- [ ] **Step 5: Commit**

```bash
git add SmartMediVend/src/medical SmartMediVend/data/medical_rules.json tests/host/test_medical_rules.cpp
git commit -m "feat: implement local medicine rule engine"
```

---

### Task 5: Add atomic inventory, backup routing, and transaction journal

**Files:**
- Create: `SmartMediVend/src/inventory/InventoryManager.h`
- Create: `SmartMediVend/src/inventory/InventoryManager.cpp`
- Create: `SmartMediVend/src/storage/IKeyValueStore.h`
- Create: `SmartMediVend/src/storage/InventorySnapshot.h`
- Create: `SmartMediVend/src/storage/SettingsStore.h`
- Create: `SmartMediVend/src/storage/SettingsStore.cpp`
- Create: `tests/host/MemoryKeyValueStore.h`
- Create: `tests/host/test_inventory.cpp`

**Interfaces:**
- Produces: `InventoryManager::reserve(canonicalId)`, `commitPulse(transactionId, channel)`, `release(transactionId)`, and `resolveChannel(canonicalId)`.
- Persistence writes A/B snapshots `{schema, sequence, quantities[16], crc32}` and a bounded transaction journal.

- [ ] **Step 1: Write failing routing and recovery tests**

```cpp
TEST_CASE(main_is_used_until_empty_then_exact_backup_is_used) {
  InventoryManager inv(MemoryKeyValueStore::withStock(0, 1).withStock(13, 5));
  REQUIRE(inv.resolveChannel("SMV-PARA500") == 0);
  inv.commitPulse("tx-1", 0);
  REQUIRE(inv.resolveChannel("SMV-PARA500") == 13);
}

TEST_CASE(newest_valid_crc_snapshot_wins_and_torn_write_is_ignored) {
  auto store = MemoryKeyValueStore::withValidSlotA(7, stockAll(5))
                   .withCorruptSlotB(8, stockAll(0));
  InventoryManager inv(store);
  REQUIRE(inv.quantity(0) == 5);
}
```

- [ ] **Step 2: Run focused tests and observe missing inventory failures**

- [ ] **Step 3: Implement fixed-size snapshots, CRC32, reservation, commit, and no auto-retry**

Reserve does not decrement. `commitPulse` decrements exactly once only after a completed 500 ms pulse and records `COMMAND_SENT_UNVERIFIED`. Duplicate transaction/channel commits are idempotent. A journal found in `PULSE_STARTED` at boot is marked ambiguous/failed and never replayed.

- [ ] **Step 4: Run full tests including simulated corrupt snapshots and duplicates**

- [ ] **Step 5: Commit**

```bash
git add SmartMediVend/src/inventory SmartMediVend/src/storage tests/host
git commit -m "feat: persist inventory and route backup slots"
```

---

### Task 6: Implement local confirmation, VendGuard, relay sequencer, and multi-SKU vending

**Files:**
- Create: `SmartMediVend/src/vending/ConfirmationGate.h`
- Create: `SmartMediVend/src/vending/ConfirmationGate.cpp`
- Create: `SmartMediVend/src/vending/VendGuard.h`
- Create: `SmartMediVend/src/vending/VendGuard.cpp`
- Create: `SmartMediVend/src/vending/IGpio.h`
- Create: `SmartMediVend/src/vending/RelayDriver.h`
- Create: `SmartMediVend/src/vending/RelayDriver.cpp`
- Create: `SmartMediVend/src/vending/VendingManager.h`
- Create: `SmartMediVend/src/vending/VendingManager.cpp`
- Create: `tests/host/FakeGpio.h`
- Create: `tests/host/test_confirmation_vending.cpp`

**Interfaces:**
- Produces: local `ConfirmationIntent`, `VendAuthorization`, nonblocking `RelayDriver::process(nowMs)`, and `VendingManager::process(nowMs)`.
- Consumes canonical candidate IDs; only after guard authorization does inventory resolve physical channels.

- [ ] **Step 1: Write failing confirmation/relay behavior tests**

```cpp
TEST_CASE(long_or_wrong_session_confirmation_never_authorizes) {
  ConfirmationGate gate;
  gate.arm("session-a", "candidate-a", 1000, 15000);
  REQUIRE_FALSE(gate.confirm("session-b", "candidate-a", "đồng ý", 2000));
  REQUIRE_FALSE(gate.confirm("session-a", "candidate-a", "đồng ý", 17000));
}

TEST_CASE(relay_is_low_for_500ms_then_high_before_channel_changes) {
  FakeGpio io;
  RelayDriver driver(io);
  driver.begin();
  REQUIRE(io.level(17) == HIGH);
  REQUIRE(driver.startPulse(3, 1000));
  driver.process(1009); REQUIRE(io.level(17) == HIGH);
  driver.process(1010); REQUIRE(io.level(17) == LOW);
  driver.process(1509); REQUIRE(io.level(17) == LOW);
  driver.process(1510); REQUIRE(io.level(17) == HIGH);
  REQUIRE(driver.takeCompletedChannel() == 3);
}

TEST_CASE(three_medicines_are_pulsed_sequentially_and_stock_decrements_once_each) {
  auto result = runVend({"SMV-PARA500", "SMV-DXM15", "SMV-DEQ025"});
  REQUIRE(result.pulseOrder == std::vector<int>({0, 3, 5}));
  REQUIRE(result.remaining[0] == 4);
  REQUIRE(result.remaining[3] == 4);
  REQUIRE(result.remaining[5] == 4);
}
```

- [ ] **Step 2: Run focused tests and observe missing vending classes**

- [ ] **Step 3: Implement the state machines**

Relay states are `SafeHigh`, `Settling`, `ActiveLow`, `GuardGap`, `Fault`. Selection occurs only while HIGH. Constants are settle=10 ms, pulse=500 ms, gap=100 ms. `VendGuard` checks state, candidate/session/version, confirmation timeout, approval mode, stock, channel 0–15, health, duplicate transaction, and no active transaction.

Production lock behavior: when pharmacist artifact is unapproved, the full evaluation and candidate UI operate but `VendGuard` returns `PharmacistApprovalRequired`; a compile-time simulator switch may route to `FakeGpio` only, never physical GPIO.

- [ ] **Step 4: Run tests and mutation-check pulse duration/duplicate commit**

Change 500 to 499 and observe the boundary test fail; restore and rerun all tests.

- [ ] **Step 5: Commit**

```bash
git add SmartMediVend/src/vending tests/host
git commit -m "feat: guard and sequence medicine vending"
```

---

### Task 7: Parse Xiaozhi protocol frames and expose safe MCP tools

**Files:**
- Create: `SmartMediVend/src/protocol/XiaozhiMessage.h`
- Create: `SmartMediVend/src/protocol/XiaozhiProtocol.h`
- Create: `SmartMediVend/src/protocol/XiaozhiProtocol.cpp`
- Create: `SmartMediVend/src/mcp/McpServer.h`
- Create: `SmartMediVend/src/mcp/McpServer.cpp`
- Create: `SmartMediVend/src/mcp/SmartMediVendTools.h`
- Create: `SmartMediVend/src/mcp/SmartMediVendTools.cpp`
- Create: `tests/host/test_protocol_mcp.cpp`

**Interfaces:**
- Produces validated hello/STT/TTS/LLM/MCP events and Opus packet views for versions 1/2/3.
- MCP supports JSON-RPC 2.0 `initialize`, `tools/list`, `tools/call` for read/status and symptom submission only.

- [ ] **Step 1: Write failing bounds and authority tests**

```cpp
TEST_CASE(v2_packet_rejects_short_header_and_claimed_payload_overrun) {
  REQUIRE(parseBinaryV2(bytes({0x00,0x02})).error == FrameError::TooShort);
  REQUIRE(parseBinaryV2(v2HeaderClaiming(200, payloadOf(10))).error ==
          FrameError::PayloadLengthMismatch);
}

TEST_CASE(mcp_tool_list_contains_no_vend_or_gpio_authority) {
  const auto names = toolsList();
  REQUIRE(names == std::vector<std::string>({
    "smartmedivend.get_device_status", "smartmedivend.get_inventory",
    "smartmedivend.get_medicine_info", "smartmedivend.submit_symptom_data",
    "smartmedivend.get_candidate_status"}));
}
```

- [ ] **Step 2: Run tests and observe missing parser/dispatcher failures**

- [ ] **Step 3: Implement strict parsers and bounded JSON-RPC responses**

Decode v2 network-endian 12-byte header and v3 4-byte header only after length checks. Cap text/MCP frames at 8192 bytes and arguments at 4096 bytes. Missing `jsonrpc`, `id`, method, or invalid params returns a JSON-RPC error without changing app state. `submit_symptom_data` passes text through `StructuredInputValidator`; it cannot accept candidate or relay fields.

- [ ] **Step 4: Run malformed corpus tests and full host suite**

Include empty payload, overflow sizes, unknown type, invalid UTF-8/JSON, duplicate request ID, unknown method, and oversized args.

- [ ] **Step 5: Commit**

```bash
git add SmartMediVend/src/protocol SmartMediVend/src/mcp tests/host/test_protocol_mcp.cpp
git commit -m "feat: add bounded Xiaozhi and MCP protocol core"
```

---

### Task 8: Vendor pinned Arduino dependencies and preserve existing Wi-Fi/TFT behavior

**Files:**
- Create: `SmartMediVend/src/vendor/DEPENDENCIES.md`
- Create: `SmartMediVend/src/vendor/*/LICENSE*`
- Create: pinned vendor source trees for ESP32WiFiPortal, ArduinoJson, Adafruit GFX, Adafruit ST7789, Adafruit BusIO, arduinoWebSockets, and Opus
- Create: `SmartMediVend/src/network/WiFiService.h`
- Create: `SmartMediVend/src/network/WiFiService.cpp`
- Create: `SmartMediVend/src/app/ButtonController.h`
- Create: `SmartMediVend/src/app/ButtonController.cpp`
- Create: `tests/host/test_button.cpp`
- Remove after migration: `SmartMediVend/ManageWiFiConnections.h`
- Remove after migration: `SmartMediVend/ManageWiFiConnections.cpp`

**Interfaces:**
- `WiFiService` keeps ESP32WiFiPortal as the only Wi-Fi owner.
- `ButtonController::process(rawPressed, nowMs)` emits exactly `ShortPress` or `LongPress`.

- [ ] **Step 1: Write failing button gesture tests**

```cpp
TEST_CASE(short_press_toggles_listening_only_after_release) {
  REQUIRE(runGesture({{0,false},{100,true},{150,true},{700,false}}) ==
          std::vector<ButtonEvent>({ButtonEvent::ShortPress}));
}

TEST_CASE(two_second_hold_emits_portal_once_and_suppresses_short_press) {
  REQUIRE(runGesture({{0,false},{100,true},{2135,true},{3000,false}}) ==
          std::vector<ButtonEvent>({ButtonEvent::LongPress}));
}
```

- [ ] **Step 2: Run button tests and confirm missing controller failure**

- [ ] **Step 3: Implement button controller and migrate Wi-Fi adapter**

Preserve saved credential connection, async portal, cooperative `process()`, auto reconnect, callbacks, telemetry, and the existing active-HIGH/external 4.7 kΩ pulldown wiring. Route short press to app listening toggle; long press to `requestConfigPortal()`.

- [ ] **Step 4: Snapshot dependencies and record exact repositories, commits, licenses, and local modifications**

Only include source needed by ESP32/Arduino. Ensure every vendored dependency has license text and no nested `.git`. Use secure WSS CA APIs and fixed-point Opus configuration. Run license/file inventory and ensure no token/credential enters the snapshot.

- [ ] **Step 5: Run host tests and an initial Arduino compile probe**

Expected host result: all tests pass. Arduino compile may expose adapter/include issues, which must be recorded and fixed in the owning network/audio tasks rather than suppressed.

- [ ] **Step 6: Commit**

```bash
git add SmartMediVend/src/vendor SmartMediVend/src/network/WiFiService.* SmartMediVend/src/app/ButtonController.* tests/host/test_button.cpp
git rm SmartMediVend/ManageWiFiConnections.h SmartMediVend/ManageWiFiConnections.cpp
git commit -m "build: vendor dependencies and migrate wifi input"
```

---

### Task 9: Implement secure bootstrap, activation, WSS transport, and session lifecycle

**Files:**
- Create: `SmartMediVend/src/network/IXiaozhiTransport.h`
- Create: `SmartMediVend/src/network/XiaozhiBootstrapClient.h`
- Create: `SmartMediVend/src/network/XiaozhiBootstrapClient.cpp`
- Create: `SmartMediVend/src/network/XiaozhiWebSocketTransport.h`
- Create: `SmartMediVend/src/network/XiaozhiWebSocketTransport.cpp`
- Create: `SmartMediVend/src/network/XiaozhiSession.h`
- Create: `SmartMediVend/src/network/XiaozhiSession.cpp`
- Create: `tests/host/test_bootstrap_session.cpp`

**Interfaces:**
- Bootstrap returns bounded `{url, token, protocolVersion, activation, serverTime}`.
- Transport emits connect/disconnect/text/binary/error; session owns hello and reconnect invalidation.

- [ ] **Step 1: Write failing parser/session tests using complete upstream-shaped fixtures**

```cpp
TEST_CASE(bootstrap_requires_https_wss_and_supported_protocol) {
  REQUIRE(parseBootstrap(validBootstrapFixture()).ok());
  REQUIRE(parseBootstrap(fixtureWithUrl("ws://plain.example")).error ==
          BootstrapError::InsecureEndpoint);
  REQUIRE(parseBootstrap(fixtureWithProtocol(9)).error ==
          BootstrapError::UnsupportedProtocol);
}

TEST_CASE(disconnect_invalidates_session_candidate_and_confirmation) {
  SessionFixture f = connectedAwaitingConfirmation();
  f.session.onTransportDisconnected();
  REQUIRE(f.session.state() == SessionState::Disconnected);
  REQUIRE_FALSE(f.candidate.isValid());
  REQUIRE_FALSE(f.confirmation.isArmed());
}
```

- [ ] **Step 2: Run tests and observe missing bootstrap/session failures**

- [ ] **Step 3: Implement current upstream headers and hello lifecycle**

Bootstrap endpoint defaults to `https://api.tenclass.net/xiaozhi/ota/`. Send `Activation-Version`, `Device-Id`, stable UUID `Client-Id`, `User-Agent`, `Accept-Language`, and bounded system JSON. WSS sends Authorization with Bearer normalization, Protocol-Version, Device-Id, Client-Id. Verify CA certificates; provide no production insecure fallback. Hello advertises MCP and Opus 16 kHz mono/frame duration; server hello controls downlink rate/frame duration.

- [ ] **Step 4: Add activation/cache/backoff and run tests**

Use finite timeouts and capped exponential backoff with jitter. Cache only successful versioned bootstrap data; never log token. Activation state can display code/challenge but cannot block app/relay loops.

- [ ] **Step 5: Commit**

```bash
git add SmartMediVend/src/network tests/host/test_bootstrap_session.cpp
git commit -m "feat: connect securely to Xiaozhi cloud"
```

---

### Task 10: Implement bounded I2S/Opus audio pipeline

**Files:**
- Create: `SmartMediVend/src/audio/AudioBuffers.h`
- Create: `SmartMediVend/src/audio/I2SMicrophone.h`
- Create: `SmartMediVend/src/audio/I2SMicrophone.cpp`
- Create: `SmartMediVend/src/audio/I2SSpeaker.h`
- Create: `SmartMediVend/src/audio/I2SSpeaker.cpp`
- Create: `SmartMediVend/src/audio/OpusCodec.h`
- Create: `SmartMediVend/src/audio/OpusCodec.cpp`
- Create: `SmartMediVend/src/audio/AudioService.h`
- Create: `SmartMediVend/src/audio/AudioService.cpp`
- Create: `tests/host/test_audio_core.cpp`

**Interfaces:**
- Produces fixed PCM/Opus frame pools, `convertInmp441Sample(int32_t)`, queue counters, start/stop/reconfigure methods.
- Network transport only exchanges complete Opus frames with `AudioService`.

- [ ] **Step 1: Write failing conversion, saturation, and queue policy tests**

```cpp
TEST_CASE(inmp441_24bit_left_aligned_samples_convert_and_saturate) {
  REQUIRE(convertInmp441Sample(0x7FFFFF00) == 32767);
  REQUIRE(convertInmp441Sample(static_cast<int32_t>(0x80000000)) == -32768);
  REQUIRE(convertInmp441Sample(0x00010000) == 1);
}

TEST_CASE(full_realtime_queue_drops_newest_and_counts_drop_without_allocating) {
  FixedFrameQueue<2> q;
  REQUIRE(q.push(frame(1)));
  REQUIRE(q.push(frame(2)));
  REQUIRE_FALSE(q.push(frame(3)));
  REQUIRE(q.dropCount() == 1);
}
```

- [ ] **Step 2: Run audio core tests and confirm missing conversion/queue failures**

- [ ] **Step 3: Implement fixed buffers and real Opus encode/decode wrapper**

Use 16 kHz mono uplink, negotiated downlink sample rate, fixed frame durations accepted by Opus, fixed-point codec build, and no per-frame allocation. Validate every decoder payload length. Reuse frame pools in internal RAM first; move large pool storage to PSRAM only when available.

- [ ] **Step 4: Add ESP32 I2S adapters and task lifecycle**

Use separate RX/TX standard I2S channels from Arduino-ESP32's bundled ESP-IDF APIs. RX converts INMP441 24-in-32 samples; TX writes signed PCM to MAX98357A. Tasks block with finite queue timeouts, report stack watermark/drop/underrun, and are created once. Stop/mute drains queues and prevents click-prone stale output.

- [ ] **Step 5: Run host tests and Arduino compile**

Expected: host tests pass; target compile links real Opus symbols and both I2S adapters without duplicate DMA/I2S ownership.

- [ ] **Step 6: Commit**

```bash
git add SmartMediVend/src/audio tests/host/test_audio_core.cpp
git commit -m "feat: add bounded duplex Opus audio"
```

---

### Task 11: Integrate app controller, TFT views, diagnostics, and composition root

**Files:**
- Create: `SmartMediVend/src/app/SmartMediVendApp.h`
- Create: `SmartMediVend/src/app/SmartMediVendApp.cpp`
- Create: `SmartMediVend/src/app/ConversationController.h`
- Create: `SmartMediVend/src/app/ConversationController.cpp`
- Create: `SmartMediVend/src/ui/UiController.h`
- Create: `SmartMediVend/src/ui/UiController.cpp`
- Move/modify: `SmartMediVend/DisplayManager.*` -> `SmartMediVend/src/ui/DisplayManager.*`
- Create: `SmartMediVend/src/diagnostics/HealthMonitor.h`
- Create: `SmartMediVend/src/diagnostics/HealthMonitor.cpp`
- Create: `tests/host/test_app_state.cpp`
- Modify: `SmartMediVend/SmartMediVend.ino`

**Interfaces:**
- `SmartMediVendApp::begin/process` is the sole sketch-facing API.
- `ConversationController` maps protocol events and local rule results to app events; it cannot call RelayDriver.
- `UiController` emits immutable view models; `DisplayManager` only draws.

- [ ] **Step 1: Write failing centralized transition tests**

```cpp
TEST_CASE(candidate_requires_spoken_offer_before_confirmation_state) {
  AppFixture f;
  f.dispatch(candidateReady("candidate-1"));
  REQUIRE(f.state() == AppState::Speaking);
  f.dispatch(ttsStopped());
  REQUIRE(f.state() == AppState::AwaitingConfirmation);
}

TEST_CASE(network_or_audio_error_during_confirmation_never_vends) {
  AppFixture f = awaitingConfirmation();
  f.dispatch(cloudDisconnected());
  f.dispatch(userConfirmed());
  REQUIRE(f.vendCount() == 0);
  REQUIRE(f.state() == AppState::Recovering);
}
```

- [ ] **Step 2: Run state tests and observe missing controller failures**

- [ ] **Step 3: Implement app/event routing and all approved UI states**

Map Wi-Fi/cloud/audio/medical/vending events through one reducer-like state transition function. TFT must show local candidate names and production-lock notice, not AI-supplied names. Preserve redraw-on-change and portrait ST7789 setup. Health monitor samples heap/min heap, PSRAM, RSSI, reconnects, queue drops, parse errors, vend errors, and boot reset reason at a non-spam interval.

- [ ] **Step 4: Reduce the sketch to the composition root**

```cpp
#include "src/app/SmartMediVendApp.h"
smv::SmartMediVendApp app;
void setup() { app.begin(); }
void loop() { app.process(); }
```

- [ ] **Step 5: Run all host tests and target compile**

Expected: full host suite passes; the Arduino target builds with no missing symbol, duplicate symbol, or pin conflict.

- [ ] **Step 6: Commit**

```bash
git add SmartMediVend tests/host/test_app_state.cpp
git commit -m "feat: integrate SmartMediVend application"
```

---

### Task 12: Documentation, clean build, safety audit, and final verification

**Files:**
- Modify: `README.md`
- Create: `docs/BUILD_REPORT.md`
- Create: `docs/TEST_REPORT.md`
- Create: `docs/HARDWARE_SMOKE_TEST.md`
- Create: `docs/SOAK_TEST.md`
- Modify: `.gitignore`

**Interfaces:**
- Documents exact reproducible commands, pinned versions, security setup, wiring, production lock, and honest PASS/NOT RUN results.

- [ ] **Step 1: Rewrite README in Vietnamese against the actual implementation**

Document ST7789, every GPIO, N16R8 restrictions, Wi-Fi portal gestures, Xiaozhi bootstrap/activation, TLS CA configuration, audio and vending flows, JSON files, pharmacist gate, build steps, licenses, troubleshooting, and the absence of drop sensors.

- [ ] **Step 2: Run fresh full host verification**

Run: `cmake -S tests/host -B build/host && cmake --build build/host --config Release && ctest --test-dir build/host -C Release --output-on-failure`

Record the exact test count and output in `docs/TEST_REPORT.md`.

- [ ] **Step 3: Run fresh clean Arduino build for the exact target**

Install/pin the stable Arduino-ESP32 core selected during dependency integration, remove `build/arduino`, and compile with 16 MB flash, 8 MB OPI PSRAM, USB CDC settings documented in README. Record board FQBN, core version, compiler, flash/PSRAM options, program size, RAM usage, and exit status in `docs/BUILD_REPORT.md`.

- [ ] **Step 4: Run static/safety checks**

Run `git diff --check`, medical JSON validation, credential/token regex scan, unresolved-marker scan, duplicate GPIO test, malformed protocol tests, duplicate vend tests, and source/license inventory. Inspect the diff for direct GPIO writes outside `RelayDriver`, physical vend entry points outside `VendGuard`, `setInsecure`, unbounded JSON documents, and long `delay()` calls.

- [ ] **Step 5: Detect hardware and run only tests supported by connected equipment**

If no serial ESP32-S3 is present, mark flash/TFT/portal/cloud/audio/relay tests `NOT RUN — no hardware detected`. If present, flash with relay loads disconnected or simulator mode first, observe serial boot/all-off, then follow `docs/HARDWARE_SMOKE_TEST.md`. Never mark an unobserved check PASS.

- [ ] **Step 6: Perform code review and fix every Critical/Important finding**

Review the implementation against the approved spec, the original prompt, every user override, and the complete git diff. Rerun the affected tests after each correction.

- [ ] **Step 7: Run the final verification set after all corrections**

Freshly rerun host tests, clean Arduino compile, JSON validation, `git diff --check`, security scan, and `git status --short`. Update reports only from these final outputs.

- [ ] **Step 8: Commit final docs and reports**

```bash
git add README.md docs .gitignore
git commit -m "docs: document SmartMediVend build and safety"
```

## Self-Review Results

- Spec coverage: every design section maps to Tasks 1–12; hardware, rules, inventory, confirmation, relay timing, Xiaozhi, MCP, audio, UI, persistence, diagnostics, documentation, build, and honest hardware status are covered.
- Placeholder scan: the plan contains no deferred implementation markers; every task names concrete files, interfaces, tests, commands, expected failures, implementation behavior, and commit scope.
- Type consistency: canonical medicine IDs flow from `MedicalRuleEngine` through candidate/confirmation/guard; physical channels are resolved only by `InventoryManager` after authorization; only `RelayDriver` receives a channel.
- Execution choice: inline execution in the current checkout, explicitly selected by the user; no branch/worktree or subagent delegation.

