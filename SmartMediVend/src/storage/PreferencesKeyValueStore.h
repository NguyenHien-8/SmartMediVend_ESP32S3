#pragma once

#include "IKeyValueStore.h"

#ifdef ARDUINO
#include <Preferences.h>
#endif

namespace smv::storage {

class PreferencesKeyValueStore final : public IKeyValueStore {
 public:
  ~PreferencesKeyValueStore() override;
  bool read(const char* key,
            uint8_t* destination,
            std::size_t capacity,
            std::size_t& bytesRead) const override;
  bool write(const char* key,
             const uint8_t* source,
             std::size_t length) override;

 private:
  bool ensureOpen() const;
#ifdef ARDUINO
  mutable Preferences preferences_;
#endif
  mutable bool opened_ = false;
};

}  // namespace smv::storage
