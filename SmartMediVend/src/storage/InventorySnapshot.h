#pragma once

#include <array>
#include <cstdint>

namespace smv::storage {

inline constexpr uint16_t kInventorySchemaVersion = 1;
inline constexpr std::size_t kInventoryChannelCount = 16;
inline constexpr std::size_t kRecentCommitCount = 8;

struct CommitRecord {
  uint64_t transactionHash = 0;
  uint16_t channelMask = 0;
};

struct InventorySnapshot {
  uint16_t schemaVersion = kInventorySchemaVersion;
  uint32_t sequence = 0;
  std::array<uint8_t, kInventoryChannelCount> quantities{};
  std::array<CommitRecord, kRecentCommitCount> recentCommits{};
  uint8_t nextCommitIndex = 0;
};

}  // namespace smv::storage
