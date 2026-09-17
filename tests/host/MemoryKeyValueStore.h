#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

#include "src/storage/IKeyValueStore.h"

namespace smv::test {

class MemoryKeyValueStore final : public storage::IKeyValueStore {
 public:
  bool read(const char* key,
            uint8_t* destination,
            std::size_t capacity,
            std::size_t& bytesRead) const override {
    const auto found = values_.find(key);
    if (found == values_.end() || found->second.size() > capacity) {
      bytesRead = 0;
      return false;
    }
    bytesRead = found->second.size();
    std::memcpy(destination, found->second.data(), bytesRead);
    return true;
  }

  bool write(const char* key,
             const uint8_t* source,
             std::size_t length) override {
    if (failWrites_) return false;
    values_[key] = std::vector<uint8_t>(source, source + length);
    return true;
  }

  void corruptByte(const std::string& key, std::size_t offset) {
    if (values_.count(key) != 0 && offset < values_[key].size()) {
      values_[key][offset] ^= 0x5AU;
    }
  }

  void setFailWrites(bool fail) { failWrites_ = fail; }

 private:
  std::unordered_map<std::string, std::vector<uint8_t>> values_;
  bool failWrites_ = false;
};

}  // namespace smv::test
