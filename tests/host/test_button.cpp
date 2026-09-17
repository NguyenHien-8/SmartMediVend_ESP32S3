#include "TestHarness.h"

#include <cstdint>
#include <vector>

#include "src/app/ButtonController.h"

using smv::ButtonController;
using smv::ButtonEvent;

namespace {

struct Sample {
  uint32_t nowMs;
  bool pressed;
};

std::vector<ButtonEvent> runGesture(std::initializer_list<Sample> samples) {
  ButtonController button(35, 2000);
  std::vector<ButtonEvent> events;
  for (const auto sample : samples) {
    const auto event = button.process(sample.pressed, sample.nowMs);
    if (event != ButtonEvent::None) events.push_back(event);
  }
  return events;
}

}  // namespace

TEST_CASE("short press emits only after debounced release") {
  const auto events = runGesture(
      {{0, false}, {100, true}, {134, true}, {135, true},
       {700, false}, {734, false}, {735, false}});
  REQUIRE(events.size() == 1);
  REQUIRE(events[0] == ButtonEvent::ShortPress);
}

TEST_CASE("two second hold emits portal once and suppresses short press") {
  const auto events = runGesture(
      {{0, false}, {100, true}, {135, true}, {2134, true}, {2135, true},
       {2500, true}, {3000, false}, {3035, false}});
  REQUIRE(events.size() == 1);
  REQUIRE(events[0] == ButtonEvent::LongPress);
}

TEST_CASE("button bounce shorter than debounce emits nothing") {
  const auto events = runGesture(
      {{0, false}, {100, true}, {120, false}, {130, true}, {150, false},
       {200, false}});
  REQUIRE(events.empty());
}
