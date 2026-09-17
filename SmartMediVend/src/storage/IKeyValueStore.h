#pragma once

#include <cstddef>
#include <cstdint>

namespace smv::storage {

class IKeyValueStore {
 public:
  virtual ~IKeyValueStore() = default;

  virtual bool read(const char* key,
                    uint8_t* destination,
                    std::size_t capacity,
                    std::size_t& bytesRead) const = 0;
  virtual bool write(const char* key,
                     const uint8_t* source,
                     std::size_t length) = 0;
};

}  // namespace smv::storage
