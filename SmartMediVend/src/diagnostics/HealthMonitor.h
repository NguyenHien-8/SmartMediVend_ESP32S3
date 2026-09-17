#pragma once

#include <cstdint>

namespace smv::diagnostics {

struct HealthSnapshot {
  uint32_t freeHeap = 0;
  uint32_t minimumFreeHeap = 0;
  uint32_t freePsram = 0;
  int32_t rssi = -127;
  uint32_t reconnects = 0;
  uint32_t audioDrops = 0;
  uint32_t protocolErrors = 0;
  uint32_t vendErrors = 0;
};

class HealthMonitor {
 public:
  void sample(uint32_t nowMs, int32_t rssi, uint32_t audioDrops);
  void recordReconnect() { ++snapshot_.reconnects; }
  void recordProtocolError() { ++snapshot_.protocolErrors; }
  void recordVendError() { ++snapshot_.vendErrors; }
  const HealthSnapshot& snapshot() const { return snapshot_; }

 private:
  HealthSnapshot snapshot_;
  uint32_t lastSampleAtMs_ = 0;
};

}  // namespace smv::diagnostics
