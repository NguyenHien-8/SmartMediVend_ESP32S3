#include "SessionTurnGate.h"

namespace smv::medical {

bool SessionTurnGate::accept(std::string_view activeSessionId,
                             std::string_view submittedSessionId,
                             uint32_t turnId) {
  if (activeSessionId.empty() || submittedSessionId != activeSessionId) {
    return false;
  }
  if (!sessionId_.empty() && sessionId_ != activeSessionId) {
    reset();
  }
  if (hasAcceptedTurn_ && turnId <= lastTurnId_) {
    return false;
  }
  sessionId_.assign(activeSessionId);
  lastTurnId_ = turnId;
  hasAcceptedTurn_ = true;
  return true;
}

void SessionTurnGate::reset() {
  sessionId_.clear();
  lastTurnId_ = 0;
  hasAcceptedTurn_ = false;
}

}  // namespace smv::medical
