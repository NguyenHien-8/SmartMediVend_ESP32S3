#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "IKeyValueStore.h"
#include "InventorySnapshot.h"

namespace smv::storage {

class SettingsStore {
 public:
  explicit SettingsStore(IKeyValueStore& store) : store_(store) {}

  bool loadInventory(InventorySnapshot& snapshot);
  bool saveInventory(InventorySnapshot& snapshot);

 private:
  static constexpr std::size_t kEncodedInventoryBytes = 111;
  using EncodedInventory = std::array<uint8_t, kEncodedInventoryBytes>;

  bool readSnapshot(const char* key, InventorySnapshot& snapshot) const;
  static EncodedInventory encode(const InventorySnapshot& snapshot);
  static bool decode(const uint8_t* data,
                     std::size_t length,
                     InventorySnapshot& snapshot);
  static uint32_t crc32(const uint8_t* data, std::size_t length);

  IKeyValueStore& store_;
  uint32_t currentSequence_ = 0;
};

}  // namespace smv::storage
