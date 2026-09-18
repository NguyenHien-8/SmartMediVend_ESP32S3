#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "AudioBuffers.h"
#include "I2SMicrophone.h"
#include "I2SSpeaker.h"
#include "OpusCodec.h"

namespace smv::audio {

class AudioService {
 public:
  bool begin();
  void end();
  void process();
  void setListening(bool listening);
  void stopPlayback();
  bool reconfigureDownlink(uint32_t sampleRate, uint16_t frameDurationMs);

  bool pushDownlink(const uint8_t* data,
                    std::size_t size,
                    uint32_t timestamp = 0);
  bool takeUplink(OpusFrame& frame) { return uplink_.pop(frame); }

  bool listening() const { return listening_; }
  uint32_t uplinkDropCount() const { return uplink_.dropCount(); }
  uint32_t downlinkDropCount() const { return downlink_.dropCount(); }
  uint32_t decodeErrorCount() const { return decodeErrors_; }
  uint32_t speakerUnderrunCount() const { return speakerErrors_; }

 private:
  I2SMicrophone microphone_;
  I2SSpeaker speaker_;
  OpusCodec codec_;
  FixedFrameQueue<OpusFrame, 4> uplink_;
  FixedFrameQueue<OpusFrame, 6> downlink_;
  std::array<int32_t, kMaximumPcmSamples> rawMicrophone_{};
  PcmFrame microphoneFrame_{};

  // Reusable audio work buffers live in the service object instead of the
  // Arduino loopTask stack. PcmFrame is ~5.8 KB and OpusFrame is ~1.3 KB;
  // keeping them as locals made AudioService::process() require ~7.1 KB of
  // stack, leaving almost no headroom in Arduino-ESP32's default 8 KB loopTask.
  // That was enough to trigger the loopTask stack canary as soon as live
  // microphone encoding began after a short button press.
  OpusFrame workOpusFrame_{};
  PcmFrame decodedFrame_{};

  std::size_t microphoneSamples_ = 0;
  bool listening_ = false;
  bool initialized_ = false;
  uint32_t decodeErrors_ = 0;
  uint32_t speakerErrors_ = 0;
};

}  // namespace smv::audio
