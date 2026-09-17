#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace smv::audio {

constexpr std::size_t kMaximumPcmSamples = 2880;  // 60 ms at 48 kHz mono.
constexpr std::size_t kMaximumOpusBytes = 1275;

struct PcmFrame {
  std::array<int16_t, kMaximumPcmSamples> samples{};
  std::size_t count = 0;
  uint32_t sampleRate = 16000;
  uint16_t frameDurationMs = 60;
};

struct OpusFrame {
  std::array<uint8_t, kMaximumOpusBytes> data{};
  std::size_t size = 0;
  uint32_t timestamp = 0;
};

constexpr int16_t convertInmp441Sample(int32_t leftAligned24Bit) {
  // INMP441 data is signed 24-bit audio left-aligned in a 32-bit I2S slot.
  // Taking bits 31..16 maps full-scale input to signed PCM16 and preserves
  // sign extension without floating point or undefined signed overflow.
  const int32_t sample = leftAligned24Bit >> 16;
  return sample > 32767 ? 32767
                        : (sample < -32768 ? -32768
                                           : static_cast<int16_t>(sample));
}

template <typename Frame, std::size_t Capacity>
class FixedFrameQueue {
 public:
  static_assert(Capacity > 0, "Queue capacity must be positive");

  bool push(const Frame& frame) {
    if (count_ == Capacity) {
      ++dropCount_;
      return false;
    }
    frames_[tail_] = frame;
    tail_ = (tail_ + 1U) % Capacity;
    ++count_;
    return true;
  }

  bool pop(Frame& frame) {
    if (count_ == 0) return false;
    frame = frames_[head_];
    head_ = (head_ + 1U) % Capacity;
    --count_;
    return true;
  }

  void clear() {
    head_ = 0;
    tail_ = 0;
    count_ = 0;
  }

  std::size_t size() const { return count_; }
  uint32_t dropCount() const { return dropCount_; }

 private:
  std::array<Frame, Capacity> frames_{};
  std::size_t head_ = 0;
  std::size_t tail_ = 0;
  std::size_t count_ = 0;
  uint32_t dropCount_ = 0;
};

}  // namespace smv::audio
