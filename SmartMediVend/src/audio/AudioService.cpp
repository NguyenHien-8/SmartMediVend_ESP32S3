#include "AudioService.h"

#include <cstring>

namespace smv::audio {

bool AudioService::begin() {
  if (initialized_) return true;
  if (!codec_.beginEncoder(16000, 60) ||
      !codec_.beginDecoder(24000, 60) || !microphone_.begin(16000) ||
      !speaker_.begin(24000)) {
    end();
    return false;
  }
  initialized_ = true;
  return true;
}

void AudioService::end() {
  listening_ = false;
  microphone_.end();
  speaker_.end();
  codec_.end();
  uplink_.clear();
  downlink_.clear();
  microphoneSamples_ = 0;
  initialized_ = false;
}

void AudioService::process() {
  if (!initialized_) return;

  if (listening_) {
    const std::size_t wanted =
        codec_.encoderFrameSamples() - microphoneSamples_;
    const std::size_t received = microphone_.read(
        rawMicrophone_.data() + microphoneSamples_, wanted, 0);
    for (std::size_t i = 0; i < received; ++i) {
      microphoneFrame_.samples[microphoneSamples_ + i] =
          convertInmp441Sample(rawMicrophone_[microphoneSamples_ + i]);
    }
    microphoneSamples_ += received;
    if (microphoneSamples_ == codec_.encoderFrameSamples()) {
      OpusFrame encoded;
      if (codec_.encode(microphoneFrame_.samples.data(), microphoneSamples_,
                        encoded)) {
        uplink_.push(encoded);
      }
      microphoneSamples_ = 0;
    }
  }

  OpusFrame encodedDownlink;
  if (downlink_.pop(encodedDownlink)) {
    PcmFrame decoded;
    if (!codec_.decode(encodedDownlink.data.data(), encodedDownlink.size,
                       decoded)) {
      ++decodeErrors_;
    } else if (!speaker_.write(decoded.samples.data(), decoded.count, 10)) {
      ++speakerErrors_;
    }
  }
}

void AudioService::setListening(bool listening) {
  listening_ = listening;
  if (!listening_) microphoneSamples_ = 0;
}

void AudioService::stopPlayback() { downlink_.clear(); }

bool AudioService::reconfigureDownlink(uint32_t sampleRate,
                                       uint16_t frameDurationMs) {
  downlink_.clear();
  speaker_.end();
  return codec_.beginDecoder(sampleRate, frameDurationMs) &&
         speaker_.begin(sampleRate);
}

bool AudioService::pushDownlink(const uint8_t* data,
                                std::size_t size,
                                uint32_t timestamp) {
  if (data == nullptr || size == 0 || size > kMaximumOpusBytes) return false;
  OpusFrame frame;
  std::memcpy(frame.data.data(), data, size);
  frame.size = size;
  frame.timestamp = timestamp;
  return downlink_.push(frame);
}

}  // namespace smv::audio
