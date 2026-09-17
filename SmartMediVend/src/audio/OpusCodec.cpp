#include "OpusCodec.h"

#include "../../opus.h"

namespace smv::audio {
namespace {

bool validRate(uint32_t sampleRate) {
  return sampleRate == 8000U || sampleRate == 12000U ||
         sampleRate == 16000U || sampleRate == 24000U ||
         sampleRate == 48000U;
}

bool validDuration(uint16_t durationMs) {
  return durationMs == 20U || durationMs == 40U || durationMs == 60U;
}

}  // namespace

OpusCodec::~OpusCodec() { end(); }

bool OpusCodec::beginEncoder(uint32_t sampleRate,
                             uint16_t frameDurationMs) {
  if (!validRate(sampleRate) || !validDuration(frameDurationMs)) return false;
  if (encoder_ != nullptr) opus_encoder_destroy(encoder_);
  int error = OPUS_OK;
  encoder_ = opus_encoder_create(static_cast<opus_int32>(sampleRate), 1,
                                 OPUS_APPLICATION_VOIP, &error);
  if (encoder_ == nullptr || error != OPUS_OK) {
    encoder_ = nullptr;
    return false;
  }
  encoderFrameSamples_ =
      static_cast<std::size_t>(sampleRate * frameDurationMs / 1000U);
  if (encoderFrameSamples_ > kMaximumPcmSamples) {
    opus_encoder_destroy(encoder_);
    encoder_ = nullptr;
    return false;
  }
  opus_encoder_ctl(encoder_, OPUS_SET_BITRATE(24000));
  opus_encoder_ctl(encoder_, OPUS_SET_COMPLEXITY(5));
  opus_encoder_ctl(encoder_, OPUS_SET_SIGNAL(OPUS_SIGNAL_VOICE));
  return true;
}

bool OpusCodec::beginDecoder(uint32_t sampleRate,
                             uint16_t frameDurationMs) {
  if (!validRate(sampleRate) || !validDuration(frameDurationMs)) return false;
  if (decoder_ != nullptr) opus_decoder_destroy(decoder_);
  int error = OPUS_OK;
  decoder_ = opus_decoder_create(static_cast<opus_int32>(sampleRate), 1,
                                 &error);
  if (decoder_ == nullptr || error != OPUS_OK) {
    decoder_ = nullptr;
    return false;
  }
  decoderFrameSamples_ =
      static_cast<std::size_t>(sampleRate * frameDurationMs / 1000U);
  decoderSampleRate_ = sampleRate;
  decoderFrameDurationMs_ = frameDurationMs;
  if (decoderFrameSamples_ > kMaximumPcmSamples) {
    opus_decoder_destroy(decoder_);
    decoder_ = nullptr;
    return false;
  }
  return true;
}

void OpusCodec::end() {
  if (encoder_ != nullptr) {
    opus_encoder_destroy(encoder_);
    encoder_ = nullptr;
  }
  if (decoder_ != nullptr) {
    opus_decoder_destroy(decoder_);
    decoder_ = nullptr;
  }
  encoderFrameSamples_ = 0;
  decoderFrameSamples_ = 0;
}

bool OpusCodec::encode(const int16_t* pcm,
                       std::size_t samples,
                       OpusFrame& output) {
  if (encoder_ == nullptr || pcm == nullptr ||
      samples != encoderFrameSamples_) {
    return false;
  }
  const int encoded = opus_encode(
      encoder_, pcm, static_cast<int>(samples), output.data.data(),
      static_cast<opus_int32>(output.data.size()));
  if (encoded <= 0 || static_cast<std::size_t>(encoded) > output.data.size()) {
    output.size = 0;
    return false;
  }
  output.size = static_cast<std::size_t>(encoded);
  return true;
}

bool OpusCodec::decode(const uint8_t* data,
                       std::size_t size,
                       PcmFrame& output) {
  if (decoder_ == nullptr || data == nullptr || size == 0 ||
      size > kMaximumOpusBytes) {
    return false;
  }
  const int decoded = opus_decode(
      decoder_, data, static_cast<opus_int32>(size), output.samples.data(),
      static_cast<int>(decoderFrameSamples_), 0);
  if (decoded <= 0 || static_cast<std::size_t>(decoded) > output.samples.size()) {
    output.count = 0;
    return false;
  }
  output.count = static_cast<std::size_t>(decoded);
  output.sampleRate = decoderSampleRate_;
  output.frameDurationMs = decoderFrameDurationMs_;
  return true;
}

}  // namespace smv::audio
