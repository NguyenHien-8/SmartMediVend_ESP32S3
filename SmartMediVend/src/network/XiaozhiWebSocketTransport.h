#pragma once

#include <string>

#include "IXiaozhiTransport.h"

#ifdef ARDUINO
#include "../vendor/arduinoWebSockets/src/WebSocketsClient.h"
#endif

namespace smv::network {

class XiaozhiWebSocketTransport final : public IXiaozhiTransport {
 public:
  void setListener(IXiaozhiTransportListener* listener) override {
    listener_ = listener;
  }
  bool connect(const TransportConfig& config) override;
  void disconnect() override;
  void process() override;
  bool sendText(std::string_view text) override;
  bool sendBinary(const uint8_t* data, std::size_t size) override;
  bool isConnected() const override;

 private:
  IXiaozhiTransportListener* listener_ = nullptr;
  std::string headers_;
#ifdef ARDUINO
  WebSocketsClient socket_;
#endif
};

}  // namespace smv::network
