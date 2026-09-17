#include "XiaozhiBootstrapClient.h"

#include <charconv>
#include <limits>

#include "../core/JsonLite.h"

#ifdef ARDUINO
#include <HTTPClient.h>
#include <NetworkClientSecure.h>
#endif

namespace smv::network {
namespace {

using jsonlite::ValueKind;
using jsonlite::ValueView;

template <typename Integer>
bool parseInteger(const ValueView& value, Integer& result) {
  if (value.kind != ValueKind::Primitive || value.raw.empty()) return false;
  const char* begin = value.raw.data();
  const char* end = begin + value.raw.size();
  const auto parsed = std::from_chars(begin, end, result);
  return parsed.ec == std::errc{} && parsed.ptr == end;
}

bool copyString(std::string_view object,
                std::string_view key,
                std::size_t maximum,
                std::string& destination,
                bool required,
                BootstrapError& error) {
  ValueView value;
  if (!jsonlite::findMember(object, key, value)) {
    if (required) error = BootstrapError::MissingField;
    return !required;
  }
  if (value.kind != ValueKind::String || value.stringValue().empty()) {
    error = BootstrapError::MissingField;
    return false;
  }
  if (value.stringValue().size() > maximum) {
    error = BootstrapError::FieldTooLarge;
    return false;
  }
  destination.assign(value.stringValue());
  return true;
}

void parseActivation(std::string_view root, BootstrapResult& result) {
  ValueView activation;
  if (!jsonlite::findMember(root, "activation", activation) ||
      activation.kind != ValueKind::Object) {
    return;
  }
  BootstrapError ignored = BootstrapError::None;
  copyString(activation.raw, "message", 256, result.activation.message,
             false, ignored);
  ignored = BootstrapError::None;
  copyString(activation.raw, "code", 64, result.activation.code, false,
             ignored);
  ignored = BootstrapError::None;
  copyString(activation.raw, "challenge", 512,
             result.activation.challenge, false, ignored);
  ValueView timeout;
  if (jsonlite::findMember(activation.raw, "timeout_ms", timeout)) {
    uint32_t parsed = 0;
    if (parseInteger(timeout, parsed) && parsed <= 600000U) {
      result.activation.timeoutMs = parsed;
    }
  }
}

void parseServerTime(std::string_view root, BootstrapResult& result) {
  ValueView serverTime;
  if (!jsonlite::findMember(root, "server_time", serverTime) ||
      serverTime.kind != ValueKind::Object) {
    return;
  }
  ValueView timestamp;
  int64_t timestampMs = 0;
  if (!jsonlite::findMember(serverTime.raw, "timestamp", timestamp) ||
      !parseInteger(timestamp, timestampMs) || timestampMs <= 0) {
    return;
  }
  result.serverTime.timestampMs = timestampMs;
  ValueView offset;
  int32_t offsetMinutes = 0;
  if (jsonlite::findMember(serverTime.raw, "timezone_offset", offset) &&
      parseInteger(offset, offsetMinutes) && offsetMinutes >= -840 &&
      offsetMinutes <= 840) {
    result.serverTime.timezoneOffsetMinutes = offsetMinutes;
  }
  result.serverTime.present = true;
}

}  // namespace

BootstrapResult BootstrapParser::parse(std::string_view json) {
  BootstrapResult result;
  if (json.size() > kMaxResponseBytes) {
    result.error = BootstrapError::ResponseTooLarge;
    return result;
  }
  if (!jsonlite::isValidObject(json)) {
    result.error = BootstrapError::MalformedJson;
    return result;
  }

  ValueView websocket;
  if (!jsonlite::findMember(json, "websocket", websocket) ||
      websocket.kind != ValueKind::Object) {
    result.error = BootstrapError::MissingField;
    return result;
  }

  if (!copyString(websocket.raw, "url", 512, result.config.url, true,
                  result.error) ||
      !copyString(websocket.raw, "token", 1024, result.config.token, true,
                  result.error)) {
    return result;
  }
  if (result.config.url.rfind("wss://", 0) != 0) {
    result.error = BootstrapError::InsecureEndpoint;
    return result;
  }

  ValueView version;
  uint32_t protocol = 0;
  if (!jsonlite::findMember(websocket.raw, "version", version) ||
      !parseInteger(version, protocol)) {
    result.error = BootstrapError::MissingField;
    return result;
  }
  if (protocol < 1 || protocol > 3) {
    result.error = BootstrapError::UnsupportedProtocol;
    return result;
  }
  result.config.protocolVersion = static_cast<uint8_t>(protocol);

  parseActivation(json, result);
  parseServerTime(json, result);
  return result;
}

BootstrapResult XiaozhiBootstrapClient::fetch(
    const BootstrapRequest& request) const {
  BootstrapResult result;
  if (request.endpoint.rfind("https://", 0) != 0) {
    result.error = BootstrapError::InsecureEndpoint;
    return result;
  }
  if (request.rootCaPem.empty()) {
    result.error = BootstrapError::TlsConfigurationMissing;
    return result;
  }

#ifdef ARDUINO
  NetworkClientSecure client;
  client.setCACert(request.rootCaPem.c_str());
  client.setHandshakeTimeout((request.timeoutMs + 999U) / 1000U);

  HTTPClient http;
  http.setConnectTimeout(request.timeoutMs);
  http.setTimeout(request.timeoutMs);
  if (!http.begin(client, request.endpoint.c_str())) {
    result.error = BootstrapError::TransportFailure;
    return result;
  }
  http.addHeader("Activation-Version",
                 request.serialNumber.empty() ? "1" : "2");
  http.addHeader("Device-Id", request.deviceId.c_str());
  http.addHeader("Client-Id", request.clientId.c_str());
  if (!request.serialNumber.empty()) {
    http.addHeader("Serial-Number", request.serialNumber.c_str());
  }
  http.addHeader("User-Agent", request.userAgent.c_str());
  http.addHeader("Accept-Language", request.language.c_str());
  http.addHeader("Content-Type", "application/json");

  const int status = request.systemInfoJson.empty()
                         ? http.GET()
                         : http.POST(reinterpret_cast<const uint8_t*>(
                                         request.systemInfoJson.data()),
                                     request.systemInfoJson.size());
  result.httpStatus = status;
  if (status != HTTP_CODE_OK) {
    result.error = status > 0 ? BootstrapError::HttpFailure
                              : BootstrapError::TransportFailure;
    http.end();
    return result;
  }
  const int announcedSize = http.getSize();
  if (announcedSize > static_cast<int>(BootstrapParser::kMaxResponseBytes)) {
    result.error = BootstrapError::ResponseTooLarge;
    http.end();
    return result;
  }
  const String body = http.getString();
  http.end();
  result = BootstrapParser::parse(
      std::string_view(body.c_str(), body.length()));
  result.httpStatus = status;
  result.config.deviceId = request.deviceId;
  result.config.clientId = request.clientId;
  result.config.rootCaPem = request.rootCaPem;
#else
  (void)request;
  result.error = BootstrapError::TransportFailure;
#endif
  return result;
}

}  // namespace smv::network
