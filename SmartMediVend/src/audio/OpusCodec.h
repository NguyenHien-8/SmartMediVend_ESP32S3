#pragma once

#include <cstddef>
#include <cstdint>

#include "AudioBuffers.h"

struct OpusEncoder;
struct OpusDecoder;

namespace smv::audio {

class OpusCodec {
 public:
  OpusCodec() = default;
  ~OpusCodec();
  OpusCodec(const OpusCodec&) = delete;
  OpusCodec& operator=(const OpusCodec&) = delete;

  bool beginEncoder(uint32_t sampleRate = 16000,
                    uint16_t frameDurationMs = 60);
  bool beginDecoder(uint32_t sampleRate,
                    uint16_t frameDurationMs);
  void end();

  bool encode(const int16_t* pcm, std::size_t samples, OpusFrame& output);
  bool decode(const uint8_t* data, std::size_t size, PcmFrame& output);

  std::size_t encoderFrameSamples() const { return encoderFrameSamples_; }
  uint32_t decoderSampleRate() const { return decoderSampleRate_; }

 private:
  OpusEncoder* encoder_ = nullptr;
  OpusDecoder* decoder_ = nullptr;
  std::size_t encoderFrameSamples_ = 0;
  std::size_t decoderFrameSamples_ = 0;
  uint32_t decoderSampleRate_ = 0;
  uint16_t decoderFrameDurationMs_ = 0;
};

}  // namespace smv::audio
