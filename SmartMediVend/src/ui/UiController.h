#pragma once

#include <array>
#include <cstddef>
#include <string_view>

#include "DisplayManager.h"

namespace smv::ui {

class UiController {
 public:
  explicit UiController(DisplayManager& display) : display_(display) {}
  void render(AppState state,
              std::string_view detail,
              const std::array<std::string_view, 3>& medicineNames,
              std::size_t medicineCount,
              bool productionLocked);
  void invalidate() { lastState_ = AppState::Booting; }

 private:
  static const char* titleFor(AppState state);
  DisplayManager& display_;
  AppState lastState_ = AppState::Booting;
  uint32_t revision_ = 0;
};

}  // namespace smv::ui
