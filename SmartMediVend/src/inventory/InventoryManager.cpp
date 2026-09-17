#include "InventoryManager.h"

namespace smv::inventory {

InventoryManager::InventoryManager(storage::IKeyValueStore& store)
    : catalog_(medical::MedicineCatalog::builtIn()), settings_(store) {
  if (settings_.loadInventory(snapshot_)) {
    return;
  }

  snapshot_ = {};
  for (uint8_t channel = 0; channel < snapshot_.quantities.size(); ++channel) {
    const auto* slot = catalog_.slotByChannel(channel);
    snapshot_.quantities[channel] = slot == nullptr ? 0 : slot->initialStock;
  }
  settings_.saveInventory(snapshot_);
}

uint8_t InventoryManager::quantity(uint8_t channel) const {
  return channel < snapshot_.quantities.size() ? snapshot_.quantities[channel]
                                                : 0;
}

int InventoryManager::resolveChannel(std::string_view canonicalId) const {
  int backupChannel = -1;
  for (uint8_t channel = 0; channel < snapshot_.quantities.size(); ++channel) {
    const auto* slot = catalog_.slotByChannel(channel);
    if (slot == nullptr || slot->canonicalId != canonicalId) {
      continue;
    }
    if (slot->backupOfChannel < 0 && snapshot_.quantities[channel] > 0) {
      return channel;
    }
    if (slot->backupOfChannel >= 0 && snapshot_.quantities[channel] > 0) {
      backupChannel = channel;
    }
  }
  return backupChannel;
}

bool InventoryManager::hasAnyCommit(std::string_view transactionId) const {
  const uint64_t transactionHash = hashTransaction(transactionId);
  for (const auto& record : snapshot_.recentCommits) {
    if (record.transactionHash == transactionHash && record.channelMask != 0U) {
      return true;
    }
  }
  return false;
}

CommitResult InventoryManager::commitPulse(std::string_view transactionId,
                                           uint8_t channel) {
  if (channel >= snapshot_.quantities.size()) {
    return CommitResult::InvalidChannel;
  }
  const uint64_t transactionHash = hashTransaction(transactionId);
  if (wasCommitted(transactionHash, channel)) {
    return CommitResult::AlreadyCommitted;
  }
  if (snapshot_.quantities[channel] == 0) {
    return CommitResult::OutOfStock;
  }

  storage::InventorySnapshot next = snapshot_;
  --next.quantities[channel];

  const uint8_t index = next.nextCommitIndex;
  auto& record = next.recentCommits[index];
  if (record.transactionHash != transactionHash) {
    record.transactionHash = transactionHash;
    record.channelMask = 0;
  }
  record.channelMask |= static_cast<uint16_t>(1U << channel);
  next.nextCommitIndex = static_cast<uint8_t>(
      (index + 1U) % next.recentCommits.size());

  if (!settings_.saveInventory(next)) {
    return CommitResult::PersistenceError;
  }
  snapshot_ = next;
  return CommitResult::Committed;
}

uint64_t InventoryManager::hashTransaction(std::string_view transactionId) {
  uint64_t hash = 1469598103934665603ULL;
  for (const unsigned char value : transactionId) {
    hash ^= value;
    hash *= 1099511628211ULL;
  }
  return hash == 0 ? 1 : hash;
}

bool InventoryManager::wasCommitted(uint64_t transactionHash,
                                    uint8_t channel) const {
  const uint16_t mask = static_cast<uint16_t>(1U << channel);
  for (const auto& record : snapshot_.recentCommits) {
    if (record.transactionHash == transactionHash &&
        (record.channelMask & mask) != 0U) {
      return true;
    }
  }
  return false;
}

}  // namespace smv::inventory
