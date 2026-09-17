#pragma once

#include <cstddef>
#include <string>
#include <string_view>

#include "SmartMediVendTools.h"

namespace smv::mcp {

class McpServer {
 public:
  static constexpr std::size_t kMaxRequestBytes = 8192;

  explicit McpServer(SmartMediVendTools& tools) : tools_(tools) {}
  std::string handle(std::string_view request);

 private:
  static std::string error(std::string_view id,
                           int code,
                           std::string_view message);
  static std::string result(std::string_view id, std::string_view json);
  static std::string toolsListJson();

  SmartMediVendTools& tools_;
};

}  // namespace smv::mcp
