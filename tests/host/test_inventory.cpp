#include "TestHarness.h"

#include <array>

#include "MemoryKeyValueStore.h"
#include "src/inventory/InventoryManager.h"
#include "src/storage/SettingsStore.h"

using smv::inventory::CommitResult;
using smv::inventory::InventoryManager;
using smv::storage::InventorySnapshot;
using smv::storage::SettingsStore;

TEST_CASE("inventory uses main slot until empty then exact backup") {
  smv::test::MemoryKeyValueStore memory;
  InventoryManager inventory(memory);

  REQUIRE(inventory.resolveChannel("PARACETAMOL_500") == 0);
  for (int index = 0; index < 5; ++index) {
    REQUIRE(inventory.commitPulse("tx-main-" + std::to_string(index), 0) ==
            CommitResult::Committed);
  }
  REQUIRE(inventory.quantity(0) == 0);
  REQUIRE(inventory.resolveChannel("PARACETAMOL_500") == 13);
}

TEST_CASE("duplicate transaction channel commit never decrements twice") {
  smv::test::MemoryKeyValueStore memory;
  InventoryManager inventory(memory);

  REQUIRE(inventory.commitPulse("tx-duplicate", 3) == CommitResult::Committed);
  REQUIRE(inventory.commitPulse("tx-duplicate", 3) ==
          CommitResult::AlreadyCommitted);
  REQUIRE(inventory.quantity(3) == 4);
}

TEST_CASE("duplicate commit remains idempotent after manager restart") {
  smv::test::MemoryKeyValueStore memory;
  {
    InventoryManager firstBoot(memory);
    REQUIRE(firstBoot.commitPulse("tx-persisted", 7) ==
            CommitResult::Committed);
  }

  InventoryManager secondBoot(memory);
  REQUIRE(secondBoot.commitPulse("tx-persisted", 7) ==
          CommitResult::AlreadyCommitted);
  REQUIRE(secondBoot.quantity(7) == 4);
}

TEST_CASE("inventory rolls back RAM count when persistent write fails") {
  smv::test::MemoryKeyValueStore memory;
  InventoryManager inventory(memory);
  memory.setFailWrites(true);

  REQUIRE(inventory.commitPulse("tx-fail", 1) ==
          CommitResult::PersistenceError);
  REQUIRE(inventory.quantity(1) == 5);
}

TEST_CASE("newest valid CRC snapshot wins and corrupt newer slot is ignored") {
  smv::test::MemoryKeyValueStore memory;
  SettingsStore settings(memory);

  InventorySnapshot first;
  first.quantities.fill(5);
  REQUIRE(settings.saveInventory(first));

  InventorySnapshot second = first;
  second.quantities[0] = 2;
  REQUIRE(settings.saveInventory(second));

  InventorySnapshot loaded;
  REQUIRE(settings.loadInventory(loaded));
  REQUIRE(loaded.quantities[0] == 2);

  memory.corruptByte("inventory_b", 8);
  REQUIRE(settings.loadInventory(loaded));
  REQUIRE(loaded.quantities[0] == 5);
}

TEST_CASE("unknown medicine and exhausted main plus backup return no channel") {
  smv::test::MemoryKeyValueStore memory;
  InventoryManager inventory(memory);
  REQUIRE(inventory.resolveChannel("UNKNOWN") == -1);

  for (int channel : {0, 13}) {
    for (int index = 0; index < 5; ++index) {
      REQUIRE(inventory.commitPulse(
                  "tx-empty-" + std::to_string(channel) + "-" +
                      std::to_string(index),
                  static_cast<uint8_t>(channel)) == CommitResult::Committed);
    }
  }
  REQUIRE(inventory.resolveChannel("PARACETAMOL_500") == -1);
}
