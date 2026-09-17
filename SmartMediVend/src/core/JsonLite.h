#pragma once

#include <string_view>

namespace smv::jsonlite {

enum class ValueKind { Invalid = 0, String, Object, Array, Primitive };

struct ValueView {
  std::string_view raw;
  ValueKind kind = ValueKind::Invalid;

  bool valid() const { return kind != ValueKind::Invalid; }
  std::string_view stringValue() const;
};

bool isValidObject(std::string_view json);
bool findMember(std::string_view object,
                std::string_view key,
                ValueView& value);

}  // namespace smv::jsonlite
