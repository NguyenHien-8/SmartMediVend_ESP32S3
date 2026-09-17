#pragma once

#include <cstdint>
#include <string_view>

#include "SymptomSession.h"

namespace smv::medical {

enum class SymptomDecodeError : uint8_t {
  None = 0,
  TooLarge,
  MalformedJson,
  MissingSessionId,
  WrongType,
  OutOfRange,
  UnknownEnumValue,
  TooManyValues
};

struct SymptomDecodeResult {
  SymptomDecodeError error = SymptomDecodeError::None;
  SymptomSession session;
  bool ok() const { return error == SymptomDecodeError::None; }
};

class SymptomJsonDecoder {
 public:
  static SymptomDecodeResult decode(std::string_view json);
};

}  // namespace smv::medical
