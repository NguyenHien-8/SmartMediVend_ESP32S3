#include "XiaozhiWebSocketTransport.h"

#ifdef ARDUINO
#include <Arduino.h>
#endif

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
  active_ = true;
  Serial.print(F("[SMV][XIAOZHI] WSS connect host="));
  Serial.print(url.host.c_str());
  Serial.print(F(" path="));
  const std::size_t query = url.path.find('?');
  Serial.print(url.path.substr(0, query).c_str());
  Serial.print(F(" protocol="));
  Serial.println(config.protocolVersion);
  std::string authorization = config.token;
  if (!authorization.empty() && authorization.find(' ') == std::string::npos) {
    authorization.insert(0, "Bearer ");
  }
  // arduinoWebSockets::setExtraHeaders() appends its own CRLF after the
  // supplied block. Do not end this string with CRLF, otherwise the library
  // emits an empty line before its User-Agent header. That prematurely ends
  // the HTTP Upgrade headers and leaves "User-Agent: ..." as bytes after the
  // handshake, which Xiaozhi accepts at HTTP level and then closes immediately.
  headers_ = "Authorization: " + authorization + "\r\n" +
             "Protocol-Version: " +
             std::to_string(config.protocolVersion) + "\r\n" +
             "Device-Id: " + config.deviceId + "\r\n" +
             "Client-Id: " + config.clientId;
  socket_.setExtraHeaders(headers_.c_str());
  // This library interprets zero as "retry on every loop", not "disabled".
  // Keep failed TCP handshakes bounded while XiaozhiSession owns the actual
  // reconnect/backoff policy.
  socket_.setReconnectInterval(1000U);
  socket_.onEvent([this](WStype_t type, uint8_t* payload, size_t length) {
    switch (type) {
      case WStype_CONNECTED:
        Serial.println(F("[SMV][XIAOZHI] WSS transport connected"));
        if (active_ && listener_ != nullptr) {
          listener_->onTransportConnected();
        }
        break;
      case WStype_DISCONNECTED:
        Serial.print(F("[SMV][XIAOZHI] WSS transport disconnected"));
        if (payload != nullptr && length > 0U) {
          Serial.print(F(" reason="));
          const std::size_t bounded = length > 64U ? 64U : length;
          for (std::size_t index = 0; index < bounded; ++index) {
            const char character = static_cast<char>(payload[index]);
            Serial.print(character >= 0x20 && character <= 0x7E
                             ? character
                             : '?');
          }
        }
        Serial.println();
        {
          const bool notify = active_;
          active_ = false;
          if (notify && listener_ != nullptr) {
            listener_->onTransportDisconnected();
          }
        }
        break;
      case WStype_TEXT:
        if (active_ && listener_ != nullptr) {
          listener_->onTransportText(std::string_view(
              reinterpret_cast<const char*>(payload), length));
        }
        break;
      case WStype_BIN:
        if (active_ && listener_ != nullptr) {
          listener_->onTransportBinary(payload, length);
        }
        break;
      case WStype_ERROR:
        Serial.println(F("[SMV][XIAOZHI] WSS transport error"));
        {
          const bool notify = active_;
          active_ = false;
          if (notify && listener_ != nullptr) listener_->onTransportError();
        }
        break;
      default:
        break;
    }
  });
  // The official Xiaozhi implementation does not request a WebSocket
  // subprotocol; only the four documented authentication/protocol headers are
  // sent.  An unsolicited subprotocol can be accepted by the HTTP upgrade and
  // then rejected by the application server.
  socket_.beginSslWithCA(url.host.c_str(), url.port, url.path.c_str(),
                         config.rootCaPem.c_str(), "");
  return true;
#else
  (void)config;
  return false;
#endif
}

void XiaozhiWebSocketTransport::disconnect() {
#ifdef ARDUINO
  active_ = false;
  socket_.disconnect();
#endif
}

void XiaozhiWebSocketTransport::process() {
#ifdef ARDUINO
  if (active_) socket_.loop();
#endif
}

bool XiaozhiWebSocketTransport::sendText(std::string_view text) {
#ifdef ARDUINO
  return active_ && text.size() <= 8192U &&
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
  return active_ && data != nullptr && size > 0 && size <= 4096U &&
         socket_.sendBIN(data, size);
#else
  (void)data;
  (void)size;
  return false;
#endif
}

bool XiaozhiWebSocketTransport::isConnected() const {
#ifdef ARDUINO
  return active_ && const_cast<WebSocketsClient&>(socket_).isConnected();
#else
  return false;
#endif
}

}  // namespace smv::network
