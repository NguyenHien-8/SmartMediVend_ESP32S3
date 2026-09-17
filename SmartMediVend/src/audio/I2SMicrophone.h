#pragma once

#include <cstddef>
#include <cstdint>

#ifdef ARDUINO
#include <driver/i2s_std.h>
#endif

namespace smv::audio {

class I2SMicrophone {
 public:
  bool begin(uint32_t sampleRate = 16000);
  void end();
  std::size_t read(int32_t* samples,
                   std::size_t maximumSamples,
                   uint32_t timeoutMs);
  bool active() const;

 private:
#ifdef ARDUINO
  i2s_chan_handle_t channel_ = nullptr;
#endif
};

}  // namespace smv::audio
