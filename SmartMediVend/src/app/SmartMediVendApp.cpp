#include "SmartMediVendApp.h"

#include <Arduino.h>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "../../AppConfig.h"
#include "../core/Elapsed.h"
#include "../core/JsonLite.h"
#include "../medical/MedicineCatalog.h"
#include "../medical/PharmacistGate.h"
#include "../medical/SymptomJsonDecoder.h"
#include "../protocol/XiaozhiProtocol.h"

namespace smv {
namespace {

constexpr std::string_view kNoCandidateDetail =
    "Nhan ngan de bat dau/dung nghe";

void writeBe16(uint8_t* output, uint16_t value) {
  output[0] = static_cast<uint8_t>(value >> 8U);
  output[1] = static_cast<uint8_t>(value);
}

void writeBe32(uint8_t* output, uint32_t value) {
  output[0] = static_cast<uint8_t>(value >> 24U);
  output[1] = static_cast<uint8_t>(value >> 16U);
  output[2] = static_cast<uint8_t>(value >> 8U);
  output[3] = static_cast<uint8_t>(value);
}

}  // namespace

SmartMediVendApp::SmartMediVendApp()
    : ui_(display_),
      relay_(gpio_),
      session_(transport_, *this),
      conversation_(*this),
      tools_(*this),
      mcpServer_(tools_) {
  session_.setEventSink(this);
}

void SmartMediVendApp::begin() {
  Serial.begin(115200);
  delay(50);
  Serial.println();
  Serial.println(F("=== SmartMediVend ESP32-S3 N16R8 ==="));
  Serial.print(F("[SMV][BOOT] reset_reason="));
  Serial.print(static_cast<unsigned int>(esp_reset_reason()));
  Serial.print(F(" loop_stack_bytes="));
  Serial.print(static_cast<unsigned long>(getArduinoLoopTaskStackSize()));
  Serial.print(F(" stack_free_min="));
  Serial.print(static_cast<unsigned int>(uxTaskGetStackHighWaterMark(nullptr)));
  Serial.print(F(" heap_free="));
  Serial.println(ESP.getFreeHeap());

  // Relay safety is initialized before display, network, audio, or storage.
  relay_.begin();
  display_.begin();
  display_.showBootScreen();

  inventory_ = std::make_unique<inventory::InventoryManager>(keyValueStore_);
  vending_ =
      std::make_unique<vending::VendingManager>(*inventory_, relay_);

  if (!audio_.begin()) {
    Serial.println(F("[SMV] Audio initialization failed; vending remains locked"));
  }

  lastWifiConnected_ = wifi_.begin();
  lastPortalActive_ = wifi_.isPortalActive();
  display_.forceRefresh();
  display_.process(wifi_);
  renderUi(kNoCandidateDetail);
  if (lastWifiConnected_) {
    startBootstrap();
  } else {
    conversation_.dispatch(AppEventType::WifiDisconnected);
    renderUi("Chua co ket noi Wi-Fi");
  }
}

void SmartMediVendApp::process() {
  const uint32_t now = millis();
  wifi_.process();
  display_.process(wifi_);
  processButton();

  const bool connected = wifi_.isConnected();
  const bool portalActive = wifi_.isPortalActive();
  if (portalActive != lastPortalActive_) {
    lastPortalActive_ = portalActive;
    ++cloudEpoch_;
    activationPending_ = false;
    session_.close();
    if (portalActive) {
      conversation_.dispatch(AppEventType::WifiPortalStarted);
      renderUi("Wi-Fi Portal dang mo");
    } else if (connected) {
      nextBootstrapAtMs_ = now;
    }
  }
  if (connected != lastWifiConnected_) {
    lastWifiConnected_ = connected;
    if (connected) {
      startBootstrap();
    } else {
      ++cloudEpoch_;
      activationPending_ = false;
      session_.close();
      conversation_.dispatch(AppEventType::WifiDisconnected);
      renderUi("Mat ket noi Wi-Fi");
    }
  }

  processBootstrap(now);
  session_.process();
  audio_.process();
  sendUplinkAudio();
  processVending(now);

  if (confirmation_.isArmed() && confirmation_.isExpired(now)) {
    confirmation_.invalidate();
    conversation_.dispatch(AppEventType::Timeout);
    renderUi("Xac nhan da het han");
  }

  health_.sample(now, wifi_.rssi(),
                 audio_.uplinkDropCount() + audio_.downlinkDropCount());
  if (conversation_.state() != lastRenderedState_) {
    renderUi(uiDetail_);
  }
  delay(1);
}

void SmartMediVendApp::setMicrophoneListening(bool listening) {
  audio_.setListening(listening);
  if (session_.state() != network::SessionState::Ready) return;
  const std::string message =
      "{\"session_id\":\"" + jsonEscape(session_.sessionId()) +
      "\",\"type\":\"listen\",\"state\":\"" +
      (listening ? "start\",\"mode\":\"manual" : "stop") + "\"}";
  session_.sendText(message);
}

void SmartMediVendApp::requestVending() {
  if (vending_ == nullptr || candidate_.count == 0 ||
      !confirmationAccepted_) {
    conversation_.dispatch(AppEventType::VendFailed);
    return;
  }

  vending::VendRequest request;
  request.transactionId = candidateId_;
  request.count = candidate_.count;
  for (std::size_t i = 0; i < candidate_.count; ++i) {
    request.canonicalIds[i].assign(candidate_.canonicalIds[i]);
  }

  const bool reviewed = productionReviewApproved();
  vending::VendContext context;
  context.appState = AppState::AwaitingConfirmation;
  context.candidateValid = candidate_.offered();
  context.safetyAllowed = candidate_.offered();
  context.userConfirmed = true;
  context.confirmationExpired = false;
  context.pharmacistApproved = reviewed;
  context.productionVendingEnabled = config::PRODUCTION_VENDING_ENABLED;
  context.inventoryAvailable = true;
  context.relayHealthy = relay_.isHealthy();
  context.sessionMatches =
      session_.state() == network::SessionState::Ready &&
      candidateSessionId_ == session_.sessionId();

  const auto result = vending_->start(request, context, millis());
  if (result == vending::VendStartResult::Started) {
    conversation_.dispatch(AppEventType::VendStarted);
    renderUi("Dang nha tung vi, moi kenh 500 ms");
  } else {
    health_.recordVendError();
    conversation_.dispatch(AppEventType::VendFailed);
    renderUi(reviewed && config::PRODUCTION_VENDING_ENABLED
                 ? "Khong the cap thuoc"
                 : "Da khoa: can duoc si phe duyet");
  }
}

void SmartMediVendApp::invalidateCandidate() {
  candidate_ = {};
  candidateId_.clear();
  candidateSessionId_.clear();
  confirmationAccepted_ = false;
  confirmation_.invalidate();
}

void SmartMediVendApp::invalidateCloudContext() {
  invalidateCandidate();
  sessionTurnGate_.reset();
  if (vending_ != nullptr && vending_->active()) vending_->cancel();
}

void SmartMediVendApp::onSessionReady(uint32_t sampleRate,
                                      uint16_t frameDurationMs) {
  if (!audio_.reconfigureDownlink(sampleRate, frameDurationMs)) {
    health_.recordProtocolError();
    session_.close();
    scheduleCloudReconnect(millis(), "Audio cloud khong tuong thich");
    return;
  }
  bootstrapFailures_ = 0;
  Serial.print(F("[SMV][XIAOZHI] session READY sample_rate="));
  Serial.print(sampleRate);
  Serial.print(F(" frame_ms="));
  Serial.println(frameDurationMs);
  conversation_.setCloudReady();
  renderUi(kNoCandidateDetail);
}

void SmartMediVendApp::onSessionText(std::string_view text) {
  const auto parsed = protocol::XiaozhiProtocol::parseText(text);
  if (!protocol::XiaozhiProtocol::matchesActiveSession(
          parsed, session_.sessionId())) {
    health_.recordProtocolError();
    return;
  }
  switch (parsed.type) {
    case protocol::TextMessageType::Mcp:
      handleMcp(text);
      break;
    case protocol::TextMessageType::Stt:
      handleStt(parsed.text, millis());
      break;
    case protocol::TextMessageType::TtsStart:
      conversation_.dispatch(AppEventType::TtsStarted);
      renderUi("Dang thong bao ket qua cuc bo");
      break;
    case protocol::TextMessageType::TtsStop:
      if (candidate_.offered() && !candidateId_.empty()) {
        confirmation_.arm(session_.sessionId(), candidateId_, millis(),
                          config::CONFIRMATION_TIMEOUT_MS);
      }
      conversation_.dispatch(AppEventType::TtsStopped);
      renderUi(candidate_.offered() ? "Nhan ngan roi noi xac nhan"
                                    : kNoCandidateDetail);
      break;
    case protocol::TextMessageType::Malformed:
    case protocol::TextMessageType::Alert:
      health_.recordProtocolError();
      break;
    default:
      break;
  }
}

void SmartMediVendApp::onSessionAudio(const uint8_t* data,
                                      std::size_t size) {
  if (!audio_.pushDownlink(data, size)) health_.recordProtocolError();
}

void SmartMediVendApp::onSessionClosed() {
  scheduleCloudReconnect(millis(), "Mat ket noi dich vu giong noi");
}

std::string SmartMediVendApp::deviceStatusJson() const {
  return std::string("{\"state\":\"") + stateName(conversation_.state()) +
         "\",\"production_vending_enabled\":" +
         (config::PRODUCTION_VENDING_ENABLED ? "true" : "false") +
         ",\"pharmacist_approved\":" +
         (productionReviewApproved() ? "true" : "false") + "}";
}

std::string SmartMediVendApp::inventoryJson() const {
  std::string output = "{\"status\":\"estimated_command_sent_unverified\",\"slots\":[";
  for (uint8_t channel = 0; channel < 16; ++channel) {
    if (channel > 0) output += ',';
    output += "{\"channel\":" + std::to_string(channel) +
              ",\"stock\":" +
              std::to_string(inventory_ == nullptr
                                 ? 0
                                 : inventory_->quantity(channel)) +
              "}";
  }
  output += "]}";
  return output;
}

std::string SmartMediVendApp::medicineInfoJson(
    std::string_view arguments) const {
  jsonlite::ValueView canonical;
  if (!jsonlite::findMember(arguments, "canonical_id", canonical) ||
      canonical.kind != jsonlite::ValueKind::String) {
    return R"({"found":false,"reason":"canonical_id_required"})";
  }
  const auto* medicine = medical::MedicineCatalog::builtIn().findMedicine(
      canonical.stringValue());
  if (medicine == nullptr) return R"({"found":false})";
  return "{\"found\":true,\"canonical_id\":\"" +
         jsonEscape(medicine->canonicalId) + "\",\"name\":\"" +
         jsonEscape(medicine->displayName) + "\",\"active_ingredient\":\"" +
         jsonEscape(medicine->activeIngredient) + "\",\"strength\":\"" +
         jsonEscape(medicine->strength) + "\"}";
}

std::string SmartMediVendApp::submitSymptomDataJson(
    std::string_view arguments) {
  const auto decoded = medical::SymptomJsonDecoder::decode(arguments);
  if (!decoded.ok()) {
    invalidateCandidate();
    return std::string(
               "{\"accepted\":false,\"decision\":\"need_more_information\",") +
           "\"decoder_error\":" +
           std::to_string(static_cast<unsigned>(decoded.error)) + "}";
  }
  if (!sessionTurnGate_.accept(session_.sessionId(),
                               decoded.session.sessionId,
                               decoded.session.turnId)) {
    invalidateCandidate();
    return R"({"accepted":false,"decision":"need_more_information","error":"stale_or_mismatched_session_turn"})";
  }

  candidate_ = ruleEngine_.evaluate(decoded.session);
  candidateSessionId_ = decoded.session.sessionId;
  candidateId_ = candidateSessionId_ + "-" +
                 std::to_string(decoded.session.turnId);
  confirmationAccepted_ = false;
  confirmation_.invalidate();

  const char* decision = "need_more_information";
  if (candidate_.decision == medical::SafetyDecision::Deny) decision = "deny";
  if (candidate_.decision == medical::SafetyDecision::Refer) decision = "refer";
  if (candidate_.offered()) decision = "offer";

  std::string output = "{\"accepted\":true,\"local_decision\":\"";
  output += decision;
  output += "\",\"candidate_id\":\"" + jsonEscape(candidateId_) +
            "\",\"medicines\":[";
  for (std::size_t index = 0; index < candidate_.count; ++index) {
    const auto* medicine = medical::MedicineCatalog::builtIn().findMedicine(
        candidate_.canonicalIds[index]);
    if (medicine == nullptr) continue;
    if (index > 0) output += ',';
    output += "{\"canonical_id\":\"" + jsonEscape(medicine->canonicalId) +
              "\",\"name\":\"" + jsonEscape(medicine->displayName) +
              "\",\"active_ingredient\":\"" +
              jsonEscape(medicine->activeIngredient) + "\",\"strength\":\"" +
              jsonEscape(medicine->strength) + "\"}";
  }
  output += "]";
  output += candidate_.offered()
                ? R"(,"next_action":"Say the locally selected medicine names once, then ask only whether the user confirms dispensing. Never choose a SKU, channel, quantity, relay, or vend action."})"
                : R"(,"next_action":"Do not offer or dispense medicine. Ask for missing safety data or advise medical/pharmacist assessment according to local_decision."})";

  if (candidate_.offered()) {
    conversation_.dispatch(AppEventType::CandidateReady);
  } else if (candidate_.decision == medical::SafetyDecision::NeedMoreInfo) {
    conversation_.dispatch(AppEventType::NeedMoreInformation);
  } else {
    conversation_.dispatch(AppEventType::SafetyRejected);
  }
  renderUi(candidate_.offered() ? "Lua chon boi bo luat cuc bo"
                                : "Khong du dieu kien cap thuoc");
  return output;
}

std::string SmartMediVendApp::candidateStatusJson() const {
  return std::string("{\"valid\":") +
         (candidate_.offered() ? "true" : "false") +
         ",\"candidate_id\":\"" + jsonEscape(candidateId_) +
         "\",\"medicine_count\":" + std::to_string(candidate_.count) +
         "}";
}

void SmartMediVendApp::bootstrapTaskEntry(void* context) {
  auto* app = static_cast<SmartMediVendApp*>(context);
  if (app->cloudTaskOperation_ == CloudTaskOperation::ActivationPoll) {
    app->activationPollResult_ =
        app->bootstrapClient_.activate(app->bootstrapRequest_);
  } else {
    app->bootstrapResult_ =
        app->bootstrapClient_.fetch(app->bootstrapRequest_);
  }
  app->bootstrapTaskState_.store(BootstrapTaskState::Complete,
                                 std::memory_order_release);
  vTaskDelete(nullptr);
}

void SmartMediVendApp::startBootstrap() {
  if (!wifi_.isConnected() || wifi_.isPortalActive() ||
      bootstrapTaskState_.load(std::memory_order_acquire) ==
          BootstrapTaskState::Running) {
    return;
  }
  bootstrapRequest_ = {};
  bootstrapRequest_.endpoint = config::XIAOZHI_BOOTSTRAP_URL;
  bootstrapRequest_.deviceId = normalizedDeviceId();
  bootstrapRequest_.clientId = stableClientId();
  bootstrapRequest_.userAgent = "smartmedivend-esp32s3/1.0.0";
  bootstrapRequest_.language = "vi-VN";
  bootstrapRequest_.rootCaPem = config::XIAOZHI_ROOT_CA_PEM;
  bootstrapRequest_.timeoutMs = config::XIAOZHI_HTTP_TIMEOUT_MS;
  bootstrapRequest_.systemInfoJson =
      "{\"version\":2,\"language\":\"vi-VN\",\"flash_size\":" +
      std::to_string(ESP.getFlashChipSize()) +
      ",\"minimum_free_heap_size\":" + std::to_string(ESP.getMinFreeHeap()) +
      ",\"mac_address\":\"" + jsonEscape(bootstrapRequest_.deviceId) +
      "\",\"uuid\":\"" + jsonEscape(bootstrapRequest_.clientId) +
      "\",\"chip_model_name\":\"esp32s3\"," +
      "\"application\":{\"name\":\"SmartMediVend\",\"version\":\"1.0.0\"," +
      "\"idf_version\":\"arduino-esp32-3.3.11\",\"elf_sha256\":\"\"}," +
      "\"board\":{\"type\":\"smartmedivend-esp32s3-n16r8\"," +
      "\"name\":\"SmartMediVend\",\"mac\":\"" +
      jsonEscape(bootstrapRequest_.deviceId) + "\"}}";
  if (bootstrapRequest_.rootCaPem.empty()) {
    bootstrapResult_ = {};
    bootstrapResult_.error = network::BootstrapError::TlsConfigurationMissing;
    bootstrapTaskState_.store(BootstrapTaskState::Complete,
                              std::memory_order_release);
    return;
  }
  activationPending_ = false;
  conversation_.dispatch(AppEventType::CloudConnecting);
  renderUi("Dang ket noi dich vu Xiaozhi");
  Serial.print(F("[SMV][XIAOZHI] provisioning device="));
  Serial.print(bootstrapRequest_.deviceId.c_str());
  Serial.print(F(" client="));
  Serial.println(bootstrapRequest_.clientId.c_str());
  startCloudTask(CloudTaskOperation::Bootstrap);
}

void SmartMediVendApp::startActivationPoll() {
  if (!wifi_.isConnected() || wifi_.isPortalActive() ||
      !activationPending_) {
    return;
  }
  startCloudTask(CloudTaskOperation::ActivationPoll);
}

bool SmartMediVendApp::startCloudTask(CloudTaskOperation operation) {
  if (bootstrapTaskState_.load(std::memory_order_acquire) ==
      BootstrapTaskState::Running) {
    return false;
  }
  cloudTaskOperation_ = operation;
  cloudTaskEpoch_ = cloudEpoch_;
  bootstrapTaskState_.store(BootstrapTaskState::Running,
                            std::memory_order_release);
  const char* taskName = operation == CloudTaskOperation::ActivationPoll
                             ? "smv-activate"
                             : "smv-bootstrap";
  if (xTaskCreatePinnedToCore(bootstrapTaskEntry, taskName, 8192, this,
                             1, nullptr, 0) != pdPASS) {
    bootstrapResult_ = {};
    bootstrapResult_.error = network::BootstrapError::TransportFailure;
    activationPollResult_ = {};
    activationPollResult_.error = network::BootstrapError::TransportFailure;
    bootstrapTaskState_.store(BootstrapTaskState::Complete,
                              std::memory_order_release);
    return false;
  }
  return true;
}

void SmartMediVendApp::processBootstrap(uint32_t nowMs) {
  if (bootstrapTaskState_.load(std::memory_order_acquire) ==
      BootstrapTaskState::Complete) {
    bootstrapTaskState_.store(BootstrapTaskState::Idle,
                              std::memory_order_release);
    if (cloudTaskEpoch_ != cloudEpoch_ || !wifi_.isConnected() ||
        wifi_.isPortalActive()) {
      Serial.println(F("[SMV][XIAOZHI] discarded stale cloud result"));
      return;
    }
    if (cloudTaskOperation_ == CloudTaskOperation::ActivationPoll) {
      Serial.print(F("[SMV][XIAOZHI] activation HTTP="));
      Serial.print(activationPollResult_.httpStatus);
      Serial.print(F(" error="));
      Serial.println(bootstrapErrorName(activationPollResult_.error));
      if (activationPollResult_.status ==
          network::ActivationPollStatus::Activated) {
        activationPending_ = false;
        nextBootstrapAtMs_ = nowMs;
        conversation_.dispatch(AppEventType::CloudConnecting);
        renderUi("Kich hoat thanh cong, dang lam moi phien");
      } else if (activationPollResult_.status ==
                 network::ActivationPollStatus::Pending) {
        nextActivationPollAtMs_ = nowMs + 3000U;
      } else {
        nextActivationPollAtMs_ = nowMs + 10000U;
        renderUi("Dang cho kich hoat; se thu lai");
      }
    } else {
      Serial.print(F("[SMV][XIAOZHI] bootstrap result="));
      Serial.print(bootstrapErrorName(bootstrapResult_.error));
      Serial.print(F(" HTTP="));
      Serial.println(bootstrapResult_.httpStatus);
      if (bootstrapResult_.ok()) {
        if (bootstrapResult_.activation.required()) {
          const bool newCode =
              !activationPending_ ||
              uiDetail_.find(bootstrapResult_.activation.code) ==
                  std::string::npos;
          activationPending_ = true;
          if (newCode) activationStartedAtMs_ = nowMs;
          // Xiaozhi's timeout_ms is the server-side activation long-poll
          // duration, not the lifetime of the displayed code.  The HTTP read
          // timeout must exceed it or Arduino HTTPClient returns -11 before
          // the expected 202 response arrives.
          bootstrapRequest_.timeoutMs =
              network::XiaozhiBootstrapClient::activationHttpTimeoutMs(
                  bootstrapResult_.activation.timeoutMs);
          activationTimeoutMs_ = 600000U;
          nextActivationPollAtMs_ = nowMs;
          conversation_.dispatch(AppEventType::ActivationRequired);
          const std::string code = bootstrapResult_.activation.code.empty()
                                       ? "(xem Serial)"
                                       : bootstrapResult_.activation.code;
          Serial.print(F("[SMV][XIAOZHI] activation code="));
          Serial.print(code.c_str());
          Serial.print(F(" long_poll_timeout_ms="));
          Serial.println(bootstrapRequest_.timeoutMs);
          renderUi("CODE: " + code + "  xiaozhi.me");
        } else if (!session_.open(bootstrapResult_.config)) {
          scheduleCloudReconnect(nowMs, "Cau hinh WSS khong hop le");
        } else {
          sessionAttemptStartedAtMs_ = nowMs;
          renderUi("Dang bat tay WebSocket bao mat");
        }
      } else {
        conversation_.dispatch(AppEventType::CloudDisconnected);
        nextBootstrapAtMs_ = nowMs + network::XiaozhiSession::reconnectDelayMs(
                                        bootstrapFailures_, 173U);
        if (bootstrapFailures_ < 255U) ++bootstrapFailures_;
        renderUi(std::string("Xiaozhi: ") +
                 bootstrapErrorName(bootstrapResult_.error) + " HTTP " +
                 std::to_string(bootstrapResult_.httpStatus));
      }
    }
  }

  if (activationPending_ &&
      elapsedMs(nowMs, activationStartedAtMs_, activationTimeoutMs_)) {
    activationPending_ = false;
    nextBootstrapAtMs_ = nowMs;
    conversation_.dispatch(AppEventType::CloudConnecting);
    renderUi("Ma het han, dang lay ma moi");
  }

  const auto sessionState = session_.state();
  if ((sessionState == network::SessionState::Connecting ||
       sessionState == network::SessionState::AwaitingHello) &&
      elapsedMs(nowMs, sessionAttemptStartedAtMs_, 15000U)) {
    Serial.println(F("[SMV][XIAOZHI] WSS hello timeout"));
    session_.close();
    scheduleCloudReconnect(nowMs, "WSS hello timeout");
  }

  if (wifi_.isConnected() && !wifi_.isPortalActive() &&
      activationPending_ &&
      bootstrapTaskState_.load(std::memory_order_acquire) ==
          BootstrapTaskState::Idle &&
      static_cast<int32_t>(nowMs - nextActivationPollAtMs_) >= 0) {
    startActivationPoll();
    return;
  }
  if (wifi_.isConnected() &&
      !wifi_.isPortalActive() &&
      !activationPending_ &&
      bootstrapTaskState_.load(std::memory_order_acquire) ==
          BootstrapTaskState::Idle &&
      (sessionState == network::SessionState::Disconnected ||
       sessionState == network::SessionState::Error) &&
      static_cast<int32_t>(nowMs - nextBootstrapAtMs_) >= 0) {
    startBootstrap();
  }
}

void SmartMediVendApp::processButton() {
  const ButtonEvent event = wifi_.takeButtonEvent();
  if (event == ButtonEvent::LongPress) {
    session_.close();
    conversation_.dispatch(AppEventType::WifiPortalStarted);
    renderUi("Wi-Fi Portal dang mo");
    return;
  }
  if (event != ButtonEvent::ShortPress ||
      session_.state() != network::SessionState::Ready) {
    return;
  }
  if (conversation_.state() == AppState::Listening) {
    Serial.println(F("[SMV][BUTTON] short press -> stop listening"));
    conversation_.dispatch(AppEventType::StopListening);
  } else {
    Serial.print(F("[SMV][BUTTON] short press -> start listening; stack_free_min="));
    Serial.println(
        static_cast<unsigned int>(uxTaskGetStackHighWaterMark(nullptr)));
    audioStackReported_ = false;
    conversation_.dispatch(AppEventType::StartListening);
  }
  renderUi(conversation_.state() == AppState::Listening
               ? "Dang nghe... nhan ngan de dung"
               : kNoCandidateDetail);
}

void SmartMediVendApp::processVending(uint32_t nowMs) {
  if (vending_ == nullptr || !vending_->active()) return;
  vending_->process(nowMs);
  if (vending_->succeeded()) {
    conversation_.dispatch(AppEventType::VendCompleted);
    renderUi("Da gui lenh 500 ms; ton kho da tru");
  } else if (vending_->state() == vending::VendingState::Failed) {
    health_.recordVendError();
    conversation_.dispatch(AppEventType::VendFailed);
    renderUi("Cap thuoc that bai");
  }
}

void SmartMediVendApp::scheduleCloudReconnect(uint32_t nowMs,
                                              std::string_view detail) {
  health_.recordReconnect();
  conversation_.dispatch(AppEventType::CloudDisconnected);
  nextBootstrapAtMs_ = nowMs + network::XiaozhiSession::reconnectDelayMs(
                                   bootstrapFailures_, 251U);
  if (bootstrapFailures_ < 255U) ++bootstrapFailures_;
  renderUi(detail);
}

void SmartMediVendApp::sendUplinkAudio() {
  if (session_.state() != network::SessionState::Ready) return;
  if (audio_.takeUplink(uplinkWorkFrame_)) {
    if (!audioStackReported_) {
      audioStackReported_ = true;
      Serial.print(F("[SMV][AUDIO] first Opus frame; stack_free_min="));
      Serial.println(
          static_cast<unsigned int>(uxTaskGetStackHighWaterMark(nullptr)));
    }
    sendAudioFrame(uplinkWorkFrame_);
  }
}

bool SmartMediVendApp::sendAudioFrame(const audio::OpusFrame& frame) {
  const uint8_t version = bootstrapResult_.config.protocolVersion;
  if (version == 1) return session_.sendAudio(frame.data.data(), frame.size);
  std::size_t header = 0;
  if (version == 2) {
    header = 16;
    writeBe16(uplinkPacket_.data(), 2);
    writeBe16(uplinkPacket_.data() + 2, 0);
    writeBe32(uplinkPacket_.data() + 4, 0);
    writeBe32(uplinkPacket_.data() + 8, audioTimestamp_);
    writeBe32(uplinkPacket_.data() + 12, static_cast<uint32_t>(frame.size));
    audioTimestamp_ += 60U;
  } else if (version == 3) {
    header = 4;
    uplinkPacket_[0] = 0;
    uplinkPacket_[1] = 0;
    writeBe16(uplinkPacket_.data() + 2, static_cast<uint16_t>(frame.size));
  } else {
    return false;
  }
  std::copy_n(frame.data.data(), frame.size, uplinkPacket_.data() + header);
  return session_.sendAudio(uplinkPacket_.data(), header + frame.size);
}

void SmartMediVendApp::renderUi(std::string_view detail) {
  const std::string nextDetail(detail);
  uiDetail_ = nextDetail;
  std::array<std::string_view, 3> names{};
  for (std::size_t index = 0; index < candidate_.count; ++index) {
    const auto* medicine = medical::MedicineCatalog::builtIn().findMedicine(
        candidate_.canonicalIds[index]);
    if (medicine != nullptr) names[index] = medicine->displayName;
  }
  ui_.render(conversation_.state(), uiDetail_, names, candidate_.count,
             !config::PRODUCTION_VENDING_ENABLED ||
                 !productionReviewApproved());
  lastRenderedState_ = conversation_.state();
  lastRenderAtMs_ = millis();
}

void SmartMediVendApp::handleMcp(std::string_view envelope) {
  jsonlite::ValueView payload;
  if (!jsonlite::findMember(envelope, "payload", payload) ||
      payload.kind != jsonlite::ValueKind::Object) {
    health_.recordProtocolError();
    return;
  }
  const std::string response = mcpServer_.handle(payload.raw);
  const std::string wrapped =
      "{\"session_id\":\"" + jsonEscape(session_.sessionId()) +
      "\",\"type\":\"mcp\",\"payload\":" + response + "}";
  session_.sendText(wrapped);
}

void SmartMediVendApp::handleStt(std::string_view transcript,
                                 uint32_t nowMs) {
  conversation_.dispatch(AppEventType::SttReceived);
  if (!candidate_.offered() || !confirmation_.isArmed()) return;
  const auto intent = vending::ConfirmationGate::classify(transcript);
  if (intent == vending::ConfirmationIntent::Reject) {
    conversation_.dispatch(AppEventType::UserCancelled);
    renderUi("Nguoi dung da huy");
    return;
  }
  if (confirmation_.confirm(session_.sessionId(), candidateId_, transcript,
                            nowMs)) {
    confirmationAccepted_ = true;
    conversation_.dispatch(AppEventType::UserConfirmed);
  }
}

std::string SmartMediVendApp::jsonEscape(std::string_view value) {
  std::string output;
  output.reserve(value.size() + 8U);
  for (const char ch : value) {
    switch (ch) {
      case '\\': output += "\\\\"; break;
      case '"': output += "\\\""; break;
      case '\n': output += "\\n"; break;
      case '\r': output += "\\r"; break;
      case '\t': output += "\\t"; break;
      default:
        if (static_cast<unsigned char>(ch) >= 0x20U) output += ch;
        break;
    }
  }
  return output;
}

const char* SmartMediVendApp::stateName(AppState state) {
  switch (state) {
    case AppState::CloudConnecting: return "cloud_connecting";
    case AppState::Activating: return "activating";
    case AppState::Idle: return "idle";
    case AppState::Listening: return "listening";
    case AppState::Processing: return "processing";
    case AppState::Speaking: return "speaking";
    case AppState::AwaitingConfirmation: return "awaiting_confirmation";
    case AppState::Dispensing: return "dispensing";
    case AppState::Recovering: return "recovering";
    case AppState::Error: return "error";
    default: return "starting";
  }
}

std::string SmartMediVendApp::stableClientId() {
  const uint64_t id = ESP.getEfuseMac();
  char value[37]{};
  const uint32_t high = static_cast<uint32_t>(id >> 32U);
  const uint32_t low = static_cast<uint32_t>(id);
  std::snprintf(value, sizeof(value), "%08lx-%04x-4%03x-8%03x-%08lx%04x",
                static_cast<unsigned long>(high),
                static_cast<unsigned>((low >> 16U) & 0xFFFFU),
                static_cast<unsigned>((low >> 4U) & 0x0FFFU),
                static_cast<unsigned>((high >> 4U) & 0x0FFFU),
                static_cast<unsigned long>(low),
                static_cast<unsigned>(high & 0xFFFFU));
  return value;
}

std::string SmartMediVendApp::normalizedDeviceId() {
  std::string value = WiFi.macAddress().c_str();
  std::transform(value.begin(), value.end(), value.begin(), [](char ch) {
    return static_cast<char>(
        std::tolower(static_cast<unsigned char>(ch)));
  });
  return value;
}

bool SmartMediVendApp::productionReviewApproved() {
  const medical::ReviewArtifact review{
      config::PHARMACIST_APPROVED,
      config::REVIEWED_CATALOG_VERSION,
      config::REVIEWED_RULES_VERSION};
  return medical::PharmacistGate::allowsProductionVending(
      review, medical::kCatalogVersion, medical::kRulesVersion);
}

const char* SmartMediVendApp::bootstrapErrorName(
    network::BootstrapError error) {
  switch (error) {
    case network::BootstrapError::None: return "OK";
    case network::BootstrapError::ResponseTooLarge: return "RESPONSE_TOO_LARGE";
    case network::BootstrapError::MalformedJson: return "MALFORMED_JSON";
    case network::BootstrapError::MissingField: return "MISSING_FIELD";
    case network::BootstrapError::FieldTooLarge: return "FIELD_TOO_LARGE";
    case network::BootstrapError::InsecureEndpoint: return "INSECURE_ENDPOINT";
    case network::BootstrapError::UnsupportedProtocol: return "UNSUPPORTED_PROTOCOL";
    case network::BootstrapError::TlsConfigurationMissing: return "TLS_CA_MISSING";
    case network::BootstrapError::TransportFailure: return "TRANSPORT_FAILURE";
    case network::BootstrapError::HttpFailure: return "HTTP_FAILURE";
    default: return "UNKNOWN";
  }
}

}  // namespace smv
