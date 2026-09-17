#include "TestHarness.h"

#include <array>
#include <cstdint>

#include "HardwarePins.h"
#include "src/app/AppState.h"
#include "src/core/Elapsed.h"

namespace {

template <std::size_t N>
constexpr bool allUnique(const std::array<uint8_t, N>& values) {
  for (std::size_t i = 0; i < N; ++i) {
    for (std::size_t j = i + 1; j < N; ++j) {
      if (values[i] == values[j]) {
        return false;
      }
    }
  }
  return true;
}

}  // namespace

TEST_CASE("app starts in Booting and wrap-safe elapsed works") {
  REQUIRE(static_cast<uint8_t>(smv::AppState::Booting) == 0);
  REQUIRE(smv::elapsedMs(4U, 0xFFFFFFF0U, 20U));
  REQUIRE_FALSE(smv::elapsedMs(3U, 0xFFFFFFF0U, 20U));
}

TEST_CASE("final assigned hardware map has no GPIO conflicts") {
  constexpr std::array<uint8_t, 18> assigned = {
      smv::pins::TFT_CS,       smv::pins::TFT_RST,      smv::pins::TFT_DC,
      smv::pins::TFT_MOSI,     smv::pins::TFT_SCLK,     smv::pins::TFT_BL,
      smv::pins::BT_SETWIFI,   smv::pins::INMP441_SD,   smv::pins::INMP441_WS,
      smv::pins::INMP441_SCK,  smv::pins::MAX98357_LRC, smv::pins::MAX98357_BCLK,
      smv::pins::MAX98357_DIN, smv::pins::MUX_S0,       smv::pins::MUX_S1,
      smv::pins::MUX_S2,       smv::pins::MUX_S3,       smv::pins::MUX_SIG};

  REQUIRE(allUnique(assigned));
  REQUIRE(smv::pins::MUX_SIG == 17);
}
