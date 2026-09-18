#pragma once

#include "AppEvent.h"
#include "AppState.h"

namespace smv {

class IConversationActions {
 public:
  virtual ~IConversationActions() = default;
  // Controls local microphone capture. Starting capture also announces
  // the Xiaozhi listen state; stopping capture after server STT must not
  // send a second stop message, so explicit user stop is separated below.
  virtual void setMicrophoneListening(bool listening) = 0;
  virtual void sendStopListening() = 0;
  virtual void requestVending() = 0;
  virtual void invalidateCandidate() = 0;
};

class ConversationController {
 public:
  explicit ConversationController(IConversationActions& actions)
      : actions_(actions) {}

  void setCloudReady();
  void dispatch(AppEventType event);
  AppState state() const { return state_; }
  bool hasCandidate() const { return candidateValid_; }

 private:
  void invalidate();

  IConversationActions& actions_;
  AppState state_ = AppState::Booting;
  AppState stateBeforeListening_ = AppState::Idle;
  bool candidateValid_ = false;
};

}  // namespace smv
