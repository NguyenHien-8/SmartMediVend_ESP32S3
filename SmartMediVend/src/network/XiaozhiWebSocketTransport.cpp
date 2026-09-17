#include "XiaozhiWebSocketTransport.h"

namespace smv::network {
namespace {

struct ParsedWssUrl {
  std::string host;
  std::string path;
  uint16_t port = 443;
  bool valid = false;
};

ParsedWssUrl parseWssUrl(std::string_view url) {
  ParsedWssUrl parsed;
  constexpr std::string_view prefix = "wss://";
  if (url.rfind(prefix, 0) != 0) return parsed;
  url.remove_prefix(prefix.size());
  const std::size_t slash = url.find('/');
  std::string_view authority =
      slash == std::string_view::npos ? url : url.substr(0, slash);
  parsed.path = slash == std::string_view::npos ? "/" : std::string(url.substr(slash));
  const std::size_t colon = authority.rfind(':');
  if (colon != std::string_view::npos) {
    uint32_t port = 0;
    for (char ch : authority.substr(colon + 1)) {
      if (ch < '0' || ch > '9') return parsed;
      port = port * 10U + static_cast<uint32_t>(ch - '0');
      if (port > 65535U) return parsed;
    }
    if (port == 0) return parsed;
    parsed.port = static_cast<uint16_t>(port);
    authority = authority.substr(0, colon);
  }
  if (authority.empty() || authority.size() > 253U) return parsed;
  parsed.host.assign(authority);
  parsed.valid = true;
  return parsed;
}

}  // namespace

bool XiaozhiWebSocketTransport::connect(const TransportConfig& config) {
  const ParsedWssUrl url = parseWssUrl(config.url);
  if (!url.valid || config.rootCaPem.empty() || config.deviceId.empty() ||
      config.clientId.empty() || config.protocolVersion < 1 ||
      config.protocolVersion > 3) {
    return false;
  }
#ifdef ARDUINO
  std::string authorization = config.token;
  if (!authorization.empty() && authorization.find(' ') == std::string::npos) {
    authorization.insert(0, "Bearer ");
  }
  headers_ = "Authorization: " + authorization + "\r\n" +
             "Protocol-Version: " +
             std::to_string(config.protocolVersion) + "\r\n" +
             "Device-Id: " + config.deviceId + "\r\n" +
             "Client-Id: " + config.clientId + "\r\n";
  socket_.setExtraHeaders(headers_.c_str());
  socket_.setReconnectInterval(0);
  socket_.onEvent([this](WStype_t type, uint8_t* payload, size_t length) {
    if (listener_ == nullptr) return;
    switch (type) {
      case WStype_CONNECTED:
        listener_->onTransportConnected();
        break;
      case WStype_DISCONNECTED:
        listener_->onTransportDisconnected();
        break;
      case WStype_TEXT:
        listener_->onTransportText(std::string_view(
            reinterpret_cast<const char*>(payload), length));
        break;
      case WStype_BIN:
        listener_->onTransportBinary(payload, length);
        break;
      case WStype_ERROR:
        listener_->onTransportError();
        break;
      default:
        break;
    }
  });
  socket_.beginSslWithCA(url.host.c_str(), url.port, url.path.c_str(),
                         config.rootCaPem.c_str(), "xiaozhi");
  return true;
#else
  (void)config;
  return false;
#endif
}

void XiaozhiWebSocketTransport::disconnect() {
#ifdef ARDUINO
  socket_.disconnect();
#endif
}

void XiaozhiWebSocketTransport::process() {
#ifdef ARDUINO
  socket_.loop();
#endif
}

bool XiaozhiWebSocketTransport::sendText(std::string_view text) {
#ifdef ARDUINO
  return text.size() <= 8192U &&
         socket_.sendTXT(reinterpret_cast<const uint8_t*>(text.data()),
                         text.size());
#else
  (void)text;
  return false;
#endif
}

bool XiaozhiWebSocketTransport::sendBinary(const uint8_t* data,
                                            std::size_t size) {
#ifdef ARDUINO
  return data != nullptr && size > 0 && size <= 4096U &&
         socket_.sendBIN(data, size);
#else
  (void)data;
  (void)size;
  return false;
#endif
}

bool XiaozhiWebSocketTransport::isConnected() const {
#ifdef ARDUINO
  return const_cast<WebSocketsClient&>(socket_).isConnected();
#else
  return false;
#endif
}

}  // namespace smv::network
