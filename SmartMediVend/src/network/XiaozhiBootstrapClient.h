#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "IXiaozhiTransport.h"

namespace smv::network {

enum class BootstrapError : uint8_t {
  None = 0,
  ResponseTooLarge,
  MalformedJson,
  MissingField,
  FieldTooLarge,
  InsecureEndpoint,
  UnsupportedProtocol,
  TlsConfigurationMissing,
  TransportFailure,
  HttpFailure
};

struct ActivationInfo {
  std::string message;
  std::string code;
  std::string challenge;
  uint32_t timeoutMs = 0;
};

struct ServerTimeInfo {
  int64_t timestampMs = 0;
  int32_t timezoneOffsetMinutes = 0;
  bool present = false;
};

struct BootstrapResult {
  BootstrapError error = BootstrapError::None;
  int httpStatus = 0;
  TransportConfig config;
  ActivationInfo activation;
  ServerTimeInfo serverTime;
  bool ok() const { return error == BootstrapError::None; }
};

class BootstrapParser {
 public:
  static constexpr std::size_t kMaxResponseBytes = 16384;
  static BootstrapResult parse(std::string_view json);
};

struct BootstrapRequest {
  std::string endpoint = "https://api.tenclass.net/xiaozhi/ota/";
  std::string deviceId;
  std::string clientId;
  std::string serialNumber;
  std::string userAgent = "SmartMediVend/1.0";
  std::string language = "vi-VN";
  std::string systemInfoJson = "{}";
  std::string rootCaPem;
  uint32_t timeoutMs = 10000;
};

class XiaozhiBootstrapClient {
 public:
  BootstrapResult fetch(const BootstrapRequest& request) const;
};

}  // namespace smv::network
