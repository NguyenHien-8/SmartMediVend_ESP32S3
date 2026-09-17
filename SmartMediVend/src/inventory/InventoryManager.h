#pragma once

#include <array>
#include <cstdint>
#include <string_view>

#include "../medical/MedicineCatalog.h"
#include "../storage/IKeyValueStore.h"
#include "../storage/SettingsStore.h"

namespace smv::inventory {

enum class CommitResult : uint8_t {
  Committed = 0,
  AlreadyCommitted,
  InvalidChannel,
  OutOfStock,
  PersistenceError
};

class InventoryManager {
 public:
  explicit InventoryManager(storage::IKeyValueStore& store);

  uint8_t quantity(uint8_t channel) const;
  int resolveChannel(std::string_view canonicalId) const;
  bool hasAnyCommit(std::string_view transactionId) const;
  CommitResult commitPulse(std::string_view transactionId, uint8_t channel);

 private:
  static uint64_t hashTransaction(std::string_view transactionId);
  bool wasCommitted(uint64_t transactionHash, uint8_t channel) const;

  const medical::MedicineCatalog& catalog_;
  storage::SettingsStore settings_;
  storage::InventorySnapshot snapshot_;
};

}  // namespace smv::inventory
