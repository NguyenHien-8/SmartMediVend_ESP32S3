#pragma once

#include <array>
#include <atomic>
#include <memory>
#include <string>
#include <string_view>

#include "ConversationController.h"
#include "../audio/AudioService.h"
#include "../diagnostics/HealthMonitor.h"
#include "../inventory/InventoryManager.h"
#include "../mcp/McpServer.h"
#include "../mcp/SmartMediVendTools.h"
#include "../medical/MedicalRuleEngine.h"
#include "../network/WiFiService.h"
#include "../network/XiaozhiBootstrapClient.h"
#include "../network/XiaozhiSession.h"
#include "../network/XiaozhiWebSocketTransport.h"
#include "../storage/PreferencesKeyValueStore.h"
#include "../ui/DisplayManager.h"
#include "../ui/UiController.h"
#include "../vending/ArduinoGpio.h"
#include "../vending/ConfirmationGate.h"
#include "../vending/RelayDriver.h"
#include "../vending/VendingManager.h"

namespace smv {

class SmartMediVendApp final : public IConversationActions,
                               public network::ICloudSessionGuard,
                               public network::IXiaozhiSessionEvents,
                               public mcp::IToolDataSource {
 public:
  SmartMediVendApp();
  void begin();
  void process();

  void setMicrophoneListening(bool listening) override;
  void requestVending() override;
  void invalidateCandidate() override;
  void invalidateCloudContext() override;

  void onSessionReady(uint32_t sampleRate,
                      uint16_t frameDurationMs) override;
  void onSessionText(std::string_view text) override;
  void onSessionAudio(const uint8_t* data, std::size_t size) override;
  void onSessionClosed() override;

  std::string deviceStatusJson() const override;
  std::string inventoryJson() const override;
  std::string medicineInfoJson(std::string_view arguments) const override;
  std::string submitSymptomDataJson(std::string_view arguments) override;
  std::string candidateStatusJson() const override;

 private:
  enum class BootstrapTaskState : uint8_t { Idle = 0, Running, Complete };

  static void bootstrapTaskEntry(void* context);
  void startBootstrap();
  void processBootstrap(uint32_t nowMs);
  void processButton();
  void processVending(uint32_t nowMs);
  void sendUplinkAudio();
  bool sendAudioFrame(const audio::OpusFrame& frame);
  void renderUi(std::string_view detail);
  void handleMcp(std::string_view envelope);
  void handleStt(std::string_view transcript, uint32_t nowMs);

  static std::string jsonEscape(std::string_view value);
  static const char* stateName(AppState state);
  static std::string stableClientId();

  DisplayManager display_;
  ui::UiController ui_;
  WiFiService wifi_;
  audio::AudioService audio_;
  vending::ArduinoGpio gpio_;
  vending::RelayDriver relay_;
  storage::PreferencesKeyValueStore keyValueStore_;
  std::unique_ptr<inventory::InventoryManager> inventory_;
  std::unique_ptr<vending::VendingManager> vending_;

  network::XiaozhiBootstrapClient bootstrapClient_;
  network::XiaozhiWebSocketTransport transport_;
  network::XiaozhiSession session_;
  ConversationController conversation_;
  medical::MedicalRuleEngine ruleEngine_;
  vending::ConfirmationGate confirmation_;
  mcp::SmartMediVendTools tools_;
  mcp::McpServer mcpServer_;
  diagnostics::HealthMonitor health_;

  medical::CandidateResult candidate_;
  std::string candidateId_;
  std::string candidateSessionId_;
  bool confirmationAccepted_ = false;
  bool lastWifiConnected_ = false;
  AppState lastRenderedState_ = AppState::Booting;
  uint32_t lastRenderAtMs_ = 0;
  uint32_t nextBootstrapAtMs_ = 0;
  uint8_t bootstrapFailures_ = 0;
  uint32_t audioTimestamp_ = 0;

  network::BootstrapRequest bootstrapRequest_;
  network::BootstrapResult bootstrapResult_;
  std::atomic<BootstrapTaskState> bootstrapTaskState_{
      BootstrapTaskState::Idle};
};

}  // namespace smv
