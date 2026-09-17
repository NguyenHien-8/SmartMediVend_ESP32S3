#include "McpServer.h"

#include "../core/JsonLite.h"

namespace smv::mcp {

std::string McpServer::handle(std::string_view request) {
  if (request.size() > kMaxRequestBytes) {
    return error("null", -32600, "REQUEST_TOO_LARGE");
  }
  if (!jsonlite::isValidObject(request)) {
    return error("null", -32700, "PARSE_ERROR");
  }

  jsonlite::ValueView version;
  jsonlite::ValueView id;
  jsonlite::ValueView method;
  if (!jsonlite::findMember(request, "jsonrpc", version) ||
      version.stringValue() != "2.0" ||
      !jsonlite::findMember(request, "id", id) ||
      !jsonlite::findMember(request, "method", method) ||
      method.kind != jsonlite::ValueKind::String) {
    return error("null", -32600, "INVALID_REQUEST");
  }
  const std::string_view requestId = id.raw;
  const std::string_view methodName = method.stringValue();

  if (methodName == "initialize") {
    return result(requestId,
                  R"({"protocolVersion":"2024-11-05","capabilities":{"tools":{}},"serverInfo":{"name":"SmartMediVend","version":"1.0.0"}})");
  }
  if (methodName == "tools/list") {
    return result(requestId, toolsListJson());
  }
  if (methodName != "tools/call") {
    return error(requestId, -32601, "METHOD_NOT_FOUND");
  }

  jsonlite::ValueView params;
  jsonlite::ValueView name;
  if (!jsonlite::findMember(request, "params", params) ||
      params.kind != jsonlite::ValueKind::Object ||
      !jsonlite::findMember(params.raw, "name", name) ||
      name.kind != jsonlite::ValueKind::String) {
    return error(requestId, -32602, "INVALID_PARAMS");
  }
  jsonlite::ValueView arguments;
  const std::string_view argumentJson =
      jsonlite::findMember(params.raw, "arguments", arguments)
          ? arguments.raw
          : std::string_view("{}");
  if (arguments.valid() && arguments.kind != jsonlite::ValueKind::Object) {
    return error(requestId, -32602, "INVALID_PARAMS");
  }

  auto executed = tools_.execute(name.stringValue(), argumentJson);
  if (!executed.found) {
    return error(requestId, -32601, "TOOL_NOT_FOUND");
  }
  if (!executed.ok) {
    return error(requestId, -32602, executed.errorTag);
  }
  return result(requestId, executed.json);
}

std::string McpServer::error(std::string_view id,
                             int code,
                             std::string_view message) {
  return std::string("{\"jsonrpc\":\"2.0\",\"id\":") + std::string(id) +
         ",\"error\":{\"code\":" + std::to_string(code) +
         ",\"message\":\"" + std::string(message) + "\"}}";
}

std::string McpServer::result(std::string_view id, std::string_view json) {
  return std::string("{\"jsonrpc\":\"2.0\",\"id\":") + std::string(id) +
         ",\"result\":" + std::string(json) + "}";
}

std::string McpServer::toolsListJson() {
  return R"({"tools":[{"name":"smartmedivend.get_device_status","description":"Read device state","inputSchema":{"type":"object","additionalProperties":false}},{"name":"smartmedivend.get_inventory","description":"Read estimated blister stock","inputSchema":{"type":"object","additionalProperties":false}},{"name":"smartmedivend.get_medicine_info","description":"Read local catalog information","inputSchema":{"type":"object"}},{"name":"smartmedivend.submit_symptom_data","description":"Submit untrusted structured symptom observations for local evaluation","inputSchema":{"type":"object"}},{"name":"smartmedivend.get_candidate_status","description":"Read the local candidate state","inputSchema":{"type":"object","additionalProperties":false}}]})";
}

}  // namespace smv::mcp
