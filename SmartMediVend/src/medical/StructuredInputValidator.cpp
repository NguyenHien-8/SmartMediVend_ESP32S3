#include "StructuredInputValidator.h"

#include <array>
#include <cctype>

namespace smv::medical {
namespace {

struct ScanResult {
  bool wellFormed = false;
  bool hasSessionId = false;
  bool hasForbiddenField = false;
};

bool isForbidden(std::string_view key) {
  constexpr std::array<std::string_view, 5> forbidden = {
      "sku", "channel", "relay", "quantity", "vend"};
  for (const auto candidate : forbidden) {
    if (key == candidate) {
      return true;
    }
  }
  return false;
}

ScanResult scanTopLevelKeys(std::string_view json) {
  ScanResult result;
  std::size_t begin = 0;
  while (begin < json.size() &&
         std::isspace(static_cast<unsigned char>(json[begin])) != 0) {
    ++begin;
  }
  std::size_t end = json.size();
  while (end > begin &&
         std::isspace(static_cast<unsigned char>(json[end - 1])) != 0) {
    --end;
  }
  if (end - begin < 2 || json[begin] != '{' || json[end - 1] != '}') {
    return result;
  }

  int depth = 0;
  bool inString = false;
  bool escaped = false;
  std::size_t stringBegin = 0;

  for (std::size_t i = begin; i < end; ++i) {
    const char ch = json[i];
    if (inString) {
      if (escaped) {
        escaped = false;
      } else if (ch == '\\') {
        escaped = true;
      } else if (ch == '"') {
        inString = false;
        if (depth == 1) {
          std::size_t next = i + 1;
          while (next < end &&
                 std::isspace(static_cast<unsigned char>(json[next])) != 0) {
            ++next;
          }
          if (next < end && json[next] == ':') {
            const auto key = json.substr(stringBegin, i - stringBegin);
            result.hasSessionId = result.hasSessionId || key == "session_id";
            result.hasForbiddenField =
                result.hasForbiddenField || isForbidden(key);
          }
        }
      }
      continue;
    }

    if (ch == '"') {
      inString = true;
      stringBegin = i + 1;
    } else if (ch == '{' || ch == '[') {
      ++depth;
    } else if (ch == '}' || ch == ']') {
      --depth;
      if (depth < 0) {
        return result;
      }
    }
  }

  result.wellFormed = !inString && depth == 0;
  return result;
}

}  // namespace

ValidationResult StructuredInputValidator::validateEnvelope(
    std::string_view json) const {
  if (json.size() > kMaxEnvelopeBytes) {
    return {ValidationError::TooLarge};
  }
  const auto scanned = scanTopLevelKeys(json);
  if (!scanned.wellFormed) {
    return {ValidationError::MalformedJson};
  }
  if (scanned.hasForbiddenField) {
    return {ValidationError::ForbiddenAuthorityField};
  }
  if (!scanned.hasSessionId) {
    return {ValidationError::MissingSessionId};
  }
  return {};
}

}  // namespace smv::medical
