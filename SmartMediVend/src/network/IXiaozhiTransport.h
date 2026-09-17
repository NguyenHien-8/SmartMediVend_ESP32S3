#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace smv::network {

struct TransportConfig {
  std::string url;
  std::string token;
  uint8_t protocolVersion = 1;
  std::string deviceId;
  std::string clientId;
  std::string rootCaPem;
};

class IXiaozhiTransportListener {
 public:
  virtual ~IXiaozhiTransportListener() = default;
  virtual void onTransportConnected() = 0;
  virtual void onTransportDisconnected() = 0;
  virtual void onTransportText(std::string_view text) = 0;
  virtual void onTransportBinary(const uint8_t* data, std::size_t size) = 0;
  virtual void onTransportError() = 0;
};

class IXiaozhiTransport {
 public:
  virtual ~IXiaozhiTransport() = default;
  virtual void setListener(IXiaozhiTransportListener* listener) = 0;
  virtual bool connect(const TransportConfig& config) = 0;
  virtual void disconnect() = 0;
  virtual void process() = 0;
  virtual bool sendText(std::string_view text) = 0;
  virtual bool sendBinary(const uint8_t* data, std::size_t size) = 0;
  virtual bool isConnected() const = 0;
};

}  // namespace smv::network
