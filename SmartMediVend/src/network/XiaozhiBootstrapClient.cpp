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

bool parseActivation(std::string_view root, BootstrapResult& result) {
  ValueView activation;
  if (!jsonlite::findMember(root, "activation", activation)) return true;
  if (activation.kind != ValueKind::Object) {
    result.error = BootstrapError::MissingField;
    return false;
  }
  BootstrapError ignored = BootstrapError::None;
  copyString(activation.raw, "message", 256, result.activation.message,
             false, ignored);
  ValueView code;
  ValueView challenge;
  const bool hasCode = jsonlite::findMember(activation.raw, "code", code);
  const bool hasChallenge =
      jsonlite::findMember(activation.raw, "challenge", challenge);
  if (hasCode != hasChallenge) {
    result.error = BootstrapError::MissingField;
    return false;
  }
  if (hasCode) {
    if (!copyString(activation.raw, "code", 64, result.activation.code, true,
                    result.error) ||
        !copyString(activation.raw, "challenge", 512,
                    result.activation.challenge, true, result.error)) {
      return false;
    }
  }
  ValueView timeout;
  if (jsonlite::findMember(activation.raw, "timeout_ms", timeout)) {
    uint32_t parsed = 0;
    if (parseInteger(timeout, parsed) && parsed <= 600000U) {
      result.activation.timeoutMs = parsed;
    }
  }
  return true;
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

  if (!parseActivation(json, result)) return result;
  parseServerTime(json, result);

  ValueView websocket;
  if (!jsonlite::findMember(json, "websocket", websocket) ||
      websocket.kind != ValueKind::Object) {
    // The official provisioning service may withhold transport credentials
    // until activation completes.  Fetch them again after /activate returns
    // HTTP 200.
    if (result.activation.required()) return result;
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
  uint32_t protocol = 1;
  if (jsonlite::findMember(websocket.raw, "version", version) &&
      !parseInteger(version, protocol)) {
    result.error = BootstrapError::UnsupportedProtocol;
    return result;
  }
  if (protocol < 1 || protocol > 3) {
    result.error = BootstrapError::UnsupportedProtocol;
    return result;
  }
  result.config.protocolVersion = static_cast<uint8_t>(protocol);

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
                         : http.POST(reinterpret_cast<uint8_t*>(
                                         const_cast<char*>(
                                             request.systemInfoJson.data())),
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

ActivationPollStatus XiaozhiBootstrapClient::classifyActivationHttpStatus(
    int httpStatus) {
  if (httpStatus == 200) return ActivationPollStatus::Activated;
  if (httpStatus == 202) return ActivationPollStatus::Pending;
  return ActivationPollStatus::Failed;
}

uint32_t XiaozhiBootstrapClient::activationHttpTimeoutMs(
    uint32_t serverWaitMs) {
  constexpr uint32_t kDefaultServerWaitMs = 30000U;
  constexpr uint32_t kResponseMarginMs = 5000U;
  constexpr uint32_t kMinimumMs = 15000U;
  constexpr uint32_t kMaximumMs = 65000U;
  const uint32_t waitMs = serverWaitMs == 0U
                              ? kDefaultServerWaitMs
                              : serverWaitMs;
  const uint64_t candidate =
      static_cast<uint64_t>(waitMs) + kResponseMarginMs;
  if (candidate < kMinimumMs) return kMinimumMs;
  if (candidate > kMaximumMs) return kMaximumMs;
  return static_cast<uint32_t>(candidate);
}

ActivationPollResult XiaozhiBootstrapClient::activate(
    const BootstrapRequest& request) const {
  ActivationPollResult result;
  if (request.endpoint.rfind("https://", 0) != 0) {
    result.error = BootstrapError::InsecureEndpoint;
    return result;
  }
  if (request.rootCaPem.empty()) {
    result.error = BootstrapError::TlsConfigurationMissing;
    return result;
  }

#ifdef ARDUINO
  std::string url = request.endpoint;
  if (url.back() != '/') url += '/';
  url += "activate";

  NetworkClientSecure client;
  client.setCACert(request.rootCaPem.c_str());
  client.setHandshakeTimeout((request.timeoutMs + 999U) / 1000U);

  HTTPClient http;
  http.setConnectTimeout(request.timeoutMs);
  http.setTimeout(request.timeoutMs);
  if (!http.begin(client, url.c_str())) {
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

  uint8_t activationPayload[] = {'{', '}'};
  const int status = http.POST(activationPayload, sizeof(activationPayload));
  http.end();
  result.httpStatus = status;
  result.status = classifyActivationHttpStatus(status);
  if (status <= 0) {
    result.error = BootstrapError::TransportFailure;
  } else if (result.status == ActivationPollStatus::Failed) {
    result.error = BootstrapError::HttpFailure;
  }
#else
  (void)request;
  result.error = BootstrapError::TransportFailure;
#endif
  return result;
}

}  // namespace smv::network
