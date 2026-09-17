#include "ConversationController.h"

namespace smv {

void ConversationController::setCloudReady() { state_ = AppState::Idle; }

void ConversationController::dispatch(AppEventType event) {
  switch (event) {
    case AppEventType::StartListening:
      if (state_ == AppState::Idle ||
          state_ == AppState::AwaitingConfirmation) {
        stateBeforeListening_ = state_;
        state_ = AppState::Listening;
        actions_.setMicrophoneListening(true);
      }
      break;

    case AppEventType::StopListening:
      if (state_ == AppState::Listening) {
        actions_.setMicrophoneListening(false);
        state_ = stateBeforeListening_ == AppState::AwaitingConfirmation
                     ? AppState::AwaitingConfirmation
                     : AppState::Idle;
      }
      break;

    case AppEventType::SttReceived:
      if (state_ == AppState::Listening) {
        actions_.setMicrophoneListening(false);
        state_ = candidateValid_ ? AppState::AwaitingConfirmation
                                 : AppState::Processing;
      }
      break;

    case AppEventType::CandidateReady:
      candidateValid_ = true;
      actions_.setMicrophoneListening(false);
      state_ = AppState::Speaking;
      break;

    case AppEventType::TtsStarted:
      state_ = AppState::Speaking;
      break;

    case AppEventType::TtsStopped:
      if (state_ == AppState::Speaking) {
        state_ = candidateValid_ ? AppState::AwaitingConfirmation
                                 : AppState::Idle;
      }
      break;

    case AppEventType::UserConfirmed:
      if (state_ == AppState::AwaitingConfirmation && candidateValid_) {
        state_ = AppState::Dispensing;
        actions_.requestVending();
      }
      break;

    case AppEventType::UserCancelled:
    case AppEventType::SafetyRejected:
      invalidate();
      state_ = AppState::Idle;
      break;

    case AppEventType::VendCompleted:
      invalidate();
      state_ = AppState::Idle;
      break;

    case AppEventType::VendFailed:
      invalidate();
      state_ = AppState::Error;
      break;

    case AppEventType::CloudDisconnected:
    case AppEventType::WifiDisconnected:
      invalidate();
      state_ = AppState::Recovering;
      break;

    case AppEventType::WifiPortalStarted:
      invalidate();
      state_ = AppState::WifiPortal;
      break;

    case AppEventType::CloudConnected:
      state_ = AppState::Idle;
      break;

    case AppEventType::NeedMoreInformation:
      state_ = AppState::Speaking;
      break;

    case AppEventType::VendStarted:
      state_ = AppState::Dispensing;
      break;

    case AppEventType::Timeout:
      if (state_ == AppState::AwaitingConfirmation) {
        invalidate();
        state_ = AppState::Idle;
      }
      break;

    case AppEventType::None:
    case AppEventType::WifiConnected:
    default:
      break;
  }
}

void ConversationController::invalidate() {
  actions_.setMicrophoneListening(false);
  if (candidateValid_) actions_.invalidateCandidate();
  candidateValid_ = false;
}

}  // namespace smv
