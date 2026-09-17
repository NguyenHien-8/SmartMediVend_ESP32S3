#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include "IXiaozhiTransport.h"

namespace smv::network {

enum class SessionState : uint8_t {
  Disconnected = 0,
  Connecting,
  AwaitingHello,
  Ready,
  Error
};

class ICloudSessionGuard {
 public:
  virtual ~ICloudSessionGuard() = default;
  virtual void invalidateCloudContext() = 0;
};

class IXiaozhiSessionEvents {
 public:
  virtual ~IXiaozhiSessionEvents() = default;
  virtual void onSessionReady(uint32_t sampleRate,
                              uint16_t frameDurationMs) = 0;
  virtual void onSessionText(std::string_view text) = 0;
  virtual void onSessionAudio(const uint8_t* data, std::size_t size) = 0;
  virtual void onSessionClosed() = 0;
};

class XiaozhiSession final : public IXiaozhiTransportListener {
 public:
  XiaozhiSession(IXiaozhiTransport& transport, ICloudSessionGuard& guard);
  void setEventSink(IXiaozhiSessionEvents* sink) { eventSink_ = sink; }

  bool open(const TransportConfig& config);
  void close();
  void process() { transport_.process(); }
  bool sendText(std::string_view text);
  bool sendAudio(const uint8_t* data, std::size_t size);

  SessionState state() const { return state_; }
  uint32_t downlinkSampleRate() const { return downlinkSampleRate_; }
  uint16_t downlinkFrameDurationMs() const {
    return downlinkFrameDurationMs_;
  }
  const std::string& sessionId() const { return sessionId_; }

  static uint32_t reconnectDelayMs(uint8_t consecutiveFailures,
                                   uint16_t jitterMs);

  void onTransportConnected() override;
  void onTransportDisconnected() override;
  void onTransportText(std::string_view text) override;
  void onTransportBinary(const uint8_t* data, std::size_t size) override;
  void onTransportError() override;

 private:
  void failClosed();
  void invalidateOnce();
  std::string helloMessage() const;

  IXiaozhiTransport& transport_;
  ICloudSessionGuard& guard_;
  TransportConfig config_;
  SessionState state_ = SessionState::Disconnected;
  std::string sessionId_;
  uint32_t downlinkSampleRate_ = 16000;
  uint16_t downlinkFrameDurationMs_ = 60;
  bool contextInvalidated_ = true;
  IXiaozhiSessionEvents* eventSink_ = nullptr;
};

}  // namespace smv::network
