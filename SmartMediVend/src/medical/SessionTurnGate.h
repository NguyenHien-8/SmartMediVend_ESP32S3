#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace smv::medical {

class SessionTurnGate {
 public:
  bool accept(std::string_view activeSessionId,
              std::string_view submittedSessionId,
              uint32_t turnId);
  void reset();

 private:
  std::string sessionId_;
  uint32_t lastTurnId_ = 0;
  bool hasAcceptedTurn_ = false;
};

}  // namespace smv::medical
