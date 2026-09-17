#pragma once

#include <cstdint>
#include <string_view>

namespace smv::medical {

enum class ValidationError : uint8_t {
  None = 0,
  TooLarge,
  MalformedJson,
  MissingSessionId,
  ForbiddenAuthorityField
};

struct ValidationResult {
  ValidationError error = ValidationError::None;
  constexpr bool ok() const { return error == ValidationError::None; }
};

class StructuredInputValidator {
 public:
  static constexpr std::size_t kMaxEnvelopeBytes = 4096;

  ValidationResult validateEnvelope(std::string_view json) const;
};

}  // namespace smv::medical
