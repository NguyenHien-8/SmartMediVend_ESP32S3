#include "TestHarness.h"

#include <cstdint>

#include "src/audio/AudioBuffers.h"

using smv::audio::FixedFrameQueue;
using smv::audio::OpusFrame;
using smv::audio::convertInmp441Sample;

namespace {
OpusFrame frame(uint8_t value) {
  OpusFrame output;
  output.size = 1;
  output.data[0] = value;
  return output;
}
}  // namespace

TEST_CASE("INMP441 24-bit left-aligned samples convert and saturate") {
  REQUIRE(convertInmp441Sample(0x7FFFFF00) == 32767);
  REQUIRE(convertInmp441Sample(static_cast<int32_t>(0x80000000U)) == -32768);
  REQUIRE(convertInmp441Sample(0x00010000) == 1);
  REQUIRE(convertInmp441Sample(0) == 0);
}

TEST_CASE("full realtime queue drops newest and counts without allocating") {
  FixedFrameQueue<OpusFrame, 2> queue;
  REQUIRE(queue.push(frame(1)));
  REQUIRE(queue.push(frame(2)));
  REQUIRE_FALSE(queue.push(frame(3)));
  REQUIRE(queue.dropCount() == 1);
  REQUIRE(queue.size() == 2);

  OpusFrame output;
  REQUIRE(queue.pop(output));
  REQUIRE(output.data[0] == 1);
  REQUIRE(queue.pop(output));
  REQUIRE(output.data[0] == 2);
  REQUIRE_FALSE(queue.pop(output));
}

TEST_CASE("queue clear discards stale playback frames") {
  FixedFrameQueue<OpusFrame, 3> queue;
  REQUIRE(queue.push(frame(7)));
  REQUIRE(queue.push(frame(8)));
  queue.clear();
  REQUIRE(queue.size() == 0);
  OpusFrame output;
  REQUIRE_FALSE(queue.pop(output));
}
