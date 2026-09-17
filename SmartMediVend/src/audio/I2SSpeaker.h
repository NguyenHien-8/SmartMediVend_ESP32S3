#pragma once

#include <cstddef>
#include <cstdint>

#ifdef ARDUINO
#include <driver/i2s_std.h>
#endif

namespace smv::audio {

class I2SSpeaker {
 public:
  bool begin(uint32_t sampleRate);
  void end();
  bool write(const int16_t* monoSamples,
             std::size_t sampleCount,
             uint32_t timeoutMs);
  bool active() const;

 private:
#ifdef ARDUINO
  i2s_chan_handle_t channel_ = nullptr;
#endif
  uint32_t sampleRate_ = 0;
};

}  // namespace smv::audio
