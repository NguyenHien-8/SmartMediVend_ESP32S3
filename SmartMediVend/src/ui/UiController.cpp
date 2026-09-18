#include "UiController.h"

#include <Arduino.h>

namespace smv::ui {
namespace {
String toArduinoString(std::string_view text) {
  String output;
  output.reserve(text.size());
  for (char character : text) output += character;
  return output;
}
}  // namespace

void UiController::render(
    AppState state,
    std::string_view detail,
    const std::array<std::string_view, 3>& medicineNames,
    std::size_t medicineCount,
    bool productionLocked) {
  String lines;
  for (std::size_t index = 0;
       index < medicineCount && index < medicineNames.size(); ++index) {
    if (index > 0) lines += '\n';
    lines += "- ";
    lines += toArduinoString(medicineNames[index]);
  }
  display_.showApplicationStatus(
      state, String(titleFor(state)), toArduinoString(detail),
      lines, productionLocked);
  lastState_ = state;
  ++revision_;
}

const char* UiController::titleFor(AppState state) {
  switch (state) {
    case AppState::CloudConnecting: return "CLOUD CONNECT";
    case AppState::Activating: return "ACTIVATION";
    case AppState::Listening: return "LISTENING";
    case AppState::Processing: return "CHECKING";
    case AppState::Speaking: return "SPEAKING";
    case AppState::AwaitingConfirmation: return "CONFIRM?";
    case AppState::Dispensing: return "DISPENSING";
    case AppState::Recovering: return "RECONNECTING";
    case AppState::Error: return "SAFE STOP";
    case AppState::Idle: return "READY";
    default: return "STARTING";
  }
}

}  // namespace smv::ui
