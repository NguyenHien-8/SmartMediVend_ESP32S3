#include "PreferencesKeyValueStore.h"

namespace smv::storage {

PreferencesKeyValueStore::~PreferencesKeyValueStore() {
#ifdef ARDUINO
  if (opened_) preferences_.end();
#endif
}

bool PreferencesKeyValueStore::ensureOpen() const {
#ifdef ARDUINO
  if (!opened_) opened_ = preferences_.begin("smv-inventory", false);
  return opened_;
#else
  return false;
#endif
}

bool PreferencesKeyValueStore::read(const char* key,
                                    uint8_t* destination,
                                    std::size_t capacity,
                                    std::size_t& bytesRead) const {
  bytesRead = 0;
#ifdef ARDUINO
  if (!ensureOpen()) return false;
  const std::size_t length = preferences_.getBytesLength(key);
  if (length == 0 || length > capacity) return false;
  bytesRead = preferences_.getBytes(key, destination, capacity);
  return bytesRead == length;
#else
  (void)key;
  (void)destination;
  (void)capacity;
  return false;
#endif
}

bool PreferencesKeyValueStore::write(const char* key,
                                     const uint8_t* source,
                                     std::size_t length) {
#ifdef ARDUINO
  return ensureOpen() &&
         preferences_.putBytes(key, source, length) == length;
#else
  (void)key;
  (void)source;
  (void)length;
  return false;
#endif
}

}  // namespace smv::storage
