#include "HealthMonitor.h"

#ifdef ARDUINO
#include <Arduino.h>
#include <esp_heap_caps.h>
#endif

#include "../core/Elapsed.h"

namespace smv::diagnostics {

void HealthMonitor::sample(uint32_t nowMs,
                           int32_t rssi,
                           uint32_t audioDrops) {
  if (!elapsedMs(nowMs, lastSampleAtMs_, 5000U)) return;
  lastSampleAtMs_ = nowMs;
  snapshot_.rssi = rssi;
  snapshot_.audioDrops = audioDrops;
#ifdef ARDUINO
  snapshot_.freeHeap = ESP.getFreeHeap();
  snapshot_.minimumFreeHeap = ESP.getMinFreeHeap();
  snapshot_.freePsram = ESP.getFreePsram();
#endif
}

}  // namespace smv::diagnostics
