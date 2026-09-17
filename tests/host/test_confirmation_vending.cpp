#include "TestHarness.h"

#include <array>
#include <string>

#include "FakeGpio.h"
#include "MemoryKeyValueStore.h"
#include "src/app/AppState.h"
#include "src/inventory/InventoryManager.h"
#include "src/vending/ConfirmationGate.h"
#include "src/vending/RelayDriver.h"
#include "src/vending/VendGuard.h"
#include "src/vending/VendingManager.h"

using smv::AppState;
using smv::inventory::InventoryManager;
using smv::vending::ConfirmationGate;
using smv::vending::RelayDriver;
using smv::vending::VendContext;
using smv::vending::VendGuard;
using smv::vending::VendRequest;
using smv::vending::VendStartResult;
using smv::vending::VendingManager;

namespace {

VendContext allowedContext() {
  VendContext context;
  context.appState = AppState::AwaitingConfirmation;
  context.candidateValid = true;
  context.safetyAllowed = true;
  context.userConfirmed = true;
  context.confirmationExpired = false;
  context.pharmacistApproved = true;
  context.productionVendingEnabled = true;
  context.inventoryAvailable = true;
  context.relayHealthy = true;
  context.transactionActive = false;
  context.duplicateRequest = false;
  context.sessionMatches = true;
  return context;
}

VendRequest request(std::string transactionId,
                    std::initializer_list<std::string> medicines) {
  VendRequest result;
  result.transactionId = std::move(transactionId);
  for (const auto& medicine : medicines) {
    result.canonicalIds[result.count++] = medicine;
  }
  return result;
}

}  // namespace

TEST_CASE("confirmation accepts bounded local phrases only for matching live state") {
  ConfirmationGate gate;
  gate.arm("session-a", "candidate-a", 1000, 15000);
  REQUIRE_FALSE(gate.confirm("session-b", "candidate-a", "đồng ý", 2000));
  REQUIRE_FALSE(gate.confirm("session-a", "candidate-a", "đồng ý", 16000));

  gate.arm("session-a", "candidate-a", 20000, 15000);
  REQUIRE(gate.confirm("session-a", "candidate-a", "  XÁC NHẬN ", 21000));
  REQUIRE_FALSE(gate.isArmed());
}

TEST_CASE("VendGuard blocks production without pharmacist approval") {
  auto context = allowedContext();
  context.pharmacistApproved = false;
  REQUIRE_FALSE(VendGuard{}.authorize(context).allowed());
}

TEST_CASE("relay stays LOW for 500ms and returns HIGH before completion") {
  smv::test::FakeGpio io;
  RelayDriver relay(io);
  relay.begin();
  REQUIRE(io.level(smv::pins::MUX_SIG));
  REQUIRE(relay.startPulse(3, 1000));

  relay.process(1009);
  REQUIRE(io.level(smv::pins::MUX_SIG));
  relay.process(1010);
  REQUIRE_FALSE(io.level(smv::pins::MUX_SIG));
  relay.process(1509);
  REQUIRE_FALSE(io.level(smv::pins::MUX_SIG));
  relay.process(1510);
  REQUIRE(io.level(smv::pins::MUX_SIG));
  REQUIRE(relay.takeCompletedChannel() == 3);
  REQUIRE_FALSE(relay.startPulse(4, 1510));
  relay.process(1610);
  REQUIRE(relay.startPulse(4, 1610));
}

TEST_CASE("three medicines pulse sequentially and decrement after each pulse") {
  smv::test::MemoryKeyValueStore memory;
  InventoryManager inventory(memory);
  smv::test::FakeGpio io;
  RelayDriver relay(io);
  relay.begin();
  VendingManager vending(inventory, relay);

  const auto vendRequest = request(
      "tx-three", {"PARACETAMOL_500", "DEXTROMETHORPHAN_15",
                    "DEQUALINIUM_025"});
  REQUIRE(vending.start(vendRequest, allowedContext(), 0) ==
          VendStartResult::Started);
  REQUIRE(inventory.quantity(0) == 5);

  for (uint32_t now = 0; now <= 2200; now += 10) {
    vending.process(now);
  }

  REQUIRE(vending.succeeded());
  REQUIRE(io.pulseOrder.size() == 3);
  REQUIRE(io.pulseOrder[0] == 0);
  REQUIRE(io.pulseOrder[1] == 3);
  REQUIRE(io.pulseOrder[2] == 5);
  REQUIRE(inventory.quantity(0) == 4);
  REQUIRE(inventory.quantity(3) == 4);
  REQUIRE(inventory.quantity(5) == 4);
}

TEST_CASE("completed transaction cannot pulse again") {
  smv::test::MemoryKeyValueStore memory;
  InventoryManager inventory(memory);
  smv::test::FakeGpio io;
  RelayDriver relay(io);
  relay.begin();
  VendingManager vending(inventory, relay);
  const auto vendRequest = request("tx-once", {"PARACETAMOL_500"});

  REQUIRE(vending.start(vendRequest, allowedContext(), 0) ==
          VendStartResult::Started);
  for (uint32_t now = 0; now <= 700; now += 10) vending.process(now);
  REQUIRE(io.pulseOrder.size() == 1);
  REQUIRE(vending.start(vendRequest, allowedContext(), 800) ==
          VendStartResult::DuplicateTransaction);
  REQUIRE(io.pulseOrder.size() == 1);
}
