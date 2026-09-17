#include "SettingsStore.h"

#include <algorithm>

namespace smv::storage {
namespace {

constexpr char kSlotA[] = "inventory_a";
constexpr char kSlotB[] = "inventory_b";
constexpr uint8_t kMagic[] = {'S', 'M', 'V', '1'};
constexpr std::size_t kPayloadBytes = 107;

void writeU16(uint8_t* destination, uint16_t value) {
  destination[0] = static_cast<uint8_t>(value & 0xFFU);
  destination[1] = static_cast<uint8_t>((value >> 8U) & 0xFFU);
}

void writeU32(uint8_t* destination, uint32_t value) {
  for (uint8_t index = 0; index < 4; ++index) {
    destination[index] = static_cast<uint8_t>((value >> (8U * index)) & 0xFFU);
  }
}

void writeU64(uint8_t* destination, uint64_t value) {
  for (uint8_t index = 0; index < 8; ++index) {
    destination[index] = static_cast<uint8_t>((value >> (8U * index)) & 0xFFU);
  }
}

uint16_t readU16(const uint8_t* source) {
  return static_cast<uint16_t>(source[0]) |
         (static_cast<uint16_t>(source[1]) << 8U);
}

uint32_t readU32(const uint8_t* source) {
  uint32_t value = 0;
  for (uint8_t index = 0; index < 4; ++index) {
    value |= static_cast<uint32_t>(source[index]) << (8U * index);
  }
  return value;
}

uint64_t readU64(const uint8_t* source) {
  uint64_t value = 0;
  for (uint8_t index = 0; index < 8; ++index) {
    value |= static_cast<uint64_t>(source[index]) << (8U * index);
  }
  return value;
}

bool newerSequence(uint32_t left, uint32_t right) {
  return static_cast<int32_t>(left - right) > 0;
}

}  // namespace

bool SettingsStore::loadInventory(InventorySnapshot& snapshot) {
  InventorySnapshot slotA;
  InventorySnapshot slotB;
  const bool validA = readSnapshot(kSlotA, slotA);
  const bool validB = readSnapshot(kSlotB, slotB);

  if (!validA && !validB) {
    currentSequence_ = 0;
    return false;
  }
  if (validA && (!validB || newerSequence(slotA.sequence, slotB.sequence))) {
    snapshot = slotA;
  } else {
    snapshot = slotB;
  }
  currentSequence_ = snapshot.sequence;
  return true;
}

bool SettingsStore::saveInventory(InventorySnapshot& snapshot) {
  InventorySnapshot latest;
  if (currentSequence_ == 0) {
    loadInventory(latest);
  }

  InventorySnapshot next = snapshot;
  next.schemaVersion = kInventorySchemaVersion;
  next.sequence = currentSequence_ + 1U;
  const auto encoded = encode(next);
  const char* key = (next.sequence & 1U) != 0U ? kSlotA : kSlotB;
  if (!store_.write(key, encoded.data(), encoded.size())) {
    return false;
  }
  snapshot = next;
  currentSequence_ = next.sequence;
  return true;
}

bool SettingsStore::readSnapshot(const char* key,
                                 InventorySnapshot& snapshot) const {
  EncodedInventory encoded{};
  std::size_t bytesRead = 0;
  if (!store_.read(key, encoded.data(), encoded.size(), bytesRead)) {
    return false;
  }
  return decode(encoded.data(), bytesRead, snapshot);
}

SettingsStore::EncodedInventory SettingsStore::encode(
    const InventorySnapshot& snapshot) {
  EncodedInventory output{};
  std::copy(std::begin(kMagic), std::end(kMagic), output.begin());
  writeU16(output.data() + 4, snapshot.schemaVersion);
  writeU32(output.data() + 6, snapshot.sequence);
  std::copy(snapshot.quantities.begin(), snapshot.quantities.end(),
            output.begin() + 10);

  std::size_t offset = 26;
  for (const auto& record : snapshot.recentCommits) {
    writeU64(output.data() + offset, record.transactionHash);
    writeU16(output.data() + offset + 8, record.channelMask);
    offset += 10;
  }
  output[106] = snapshot.nextCommitIndex;
  writeU32(output.data() + kPayloadBytes,
           crc32(output.data(), kPayloadBytes));
  return output;
}

bool SettingsStore::decode(const uint8_t* data,
                           std::size_t length,
                           InventorySnapshot& snapshot) {
  if (length != kEncodedInventoryBytes ||
      !std::equal(std::begin(kMagic), std::end(kMagic), data)) {
    return false;
  }
  const uint32_t storedCrc = readU32(data + kPayloadBytes);
  if (storedCrc != crc32(data, kPayloadBytes)) {
    return false;
  }
  if (readU16(data + 4) != kInventorySchemaVersion) {
    return false;
  }

  snapshot = {};
  snapshot.schemaVersion = kInventorySchemaVersion;
  snapshot.sequence = readU32(data + 6);
  std::copy(data + 10, data + 26, snapshot.quantities.begin());

  std::size_t offset = 26;
  for (auto& record : snapshot.recentCommits) {
    record.transactionHash = readU64(data + offset);
    record.channelMask = readU16(data + offset + 8);
    offset += 10;
  }
  snapshot.nextCommitIndex = data[106];
  if (snapshot.nextCommitIndex >= snapshot.recentCommits.size()) {
    return false;
  }
  return true;
}

uint32_t SettingsStore::crc32(const uint8_t* data, std::size_t length) {
  uint32_t crc = 0xFFFFFFFFU;
  for (std::size_t index = 0; index < length; ++index) {
    crc ^= data[index];
    for (uint8_t bit = 0; bit < 8; ++bit) {
      const uint32_t mask = 0U - (crc & 1U);
      crc = (crc >> 1U) ^ (0xEDB88320U & mask);
    }
  }
  return ~crc;
}

}  // namespace smv::storage
