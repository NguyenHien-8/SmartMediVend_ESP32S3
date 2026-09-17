#include "TestHarness.h"

#include "src/app/ConversationController.h"

using smv::AppEventType;
using smv::AppState;
using smv::ConversationController;
using smv::IConversationActions;

namespace {

class FakeActions final : public IConversationActions {
 public:
  void setMicrophoneListening(bool listening) override {
    microphoneListening = listening;
  }
  void requestVending() override { ++vendRequests; }
  void invalidateCandidate() override { ++invalidations; }

  bool microphoneListening = false;
  int vendRequests = 0;
  int invalidations = 0;
};

}  // namespace

TEST_CASE("candidate requires spoken offer before confirmation state") {
  FakeActions actions;
  ConversationController controller(actions);
  controller.setCloudReady();
  controller.dispatch(AppEventType::CandidateReady);
  REQUIRE(controller.state() == AppState::Speaking);
  REQUIRE(controller.hasCandidate());
  controller.dispatch(AppEventType::TtsStopped);
  REQUIRE(controller.state() == AppState::AwaitingConfirmation);
  REQUIRE(actions.vendRequests == 0);
}

TEST_CASE("network error during confirmation never vends") {
  FakeActions actions;
  ConversationController controller(actions);
  controller.setCloudReady();
  controller.dispatch(AppEventType::CandidateReady);
  controller.dispatch(AppEventType::TtsStopped);
  controller.dispatch(AppEventType::CloudDisconnected);
  controller.dispatch(AppEventType::UserConfirmed);
  REQUIRE(actions.vendRequests == 0);
  REQUIRE(actions.invalidations == 1);
  REQUIRE(controller.state() == AppState::Recovering);
}

TEST_CASE("short press toggles microphone and confirmed local candidate requests vend") {
  FakeActions actions;
  ConversationController controller(actions);
  controller.setCloudReady();
  controller.dispatch(AppEventType::StartListening);
  REQUIRE(controller.state() == AppState::Listening);
  REQUIRE(actions.microphoneListening);
  controller.dispatch(AppEventType::StopListening);
  REQUIRE(controller.state() == AppState::Idle);
  REQUIRE_FALSE(actions.microphoneListening);

  controller.dispatch(AppEventType::CandidateReady);
  controller.dispatch(AppEventType::TtsStopped);
  controller.dispatch(AppEventType::StartListening);
  controller.dispatch(AppEventType::SttReceived);
  controller.dispatch(AppEventType::UserConfirmed);
  REQUIRE(actions.vendRequests == 1);
  REQUIRE(controller.state() == AppState::Dispensing);
}
