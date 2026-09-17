#include "JsonLite.h"

#include <cctype>

namespace smv::jsonlite {
namespace {

void skipWhitespace(std::string_view text, std::size_t& position) {
  while (position < text.size() &&
         std::isspace(static_cast<unsigned char>(text[position])) != 0) {
    ++position;
  }
}

bool skipString(std::string_view text, std::size_t& position) {
  if (position >= text.size() || text[position] != '"') return false;
  ++position;
  bool escaped = false;
  while (position < text.size()) {
    const char ch = text[position++];
    if (escaped) {
      escaped = false;
    } else if (ch == '\\') {
      escaped = true;
    } else if (ch == '"') {
      return true;
    } else if (static_cast<unsigned char>(ch) < 0x20U) {
      return false;
    }
  }
  return false;
}

bool skipValue(std::string_view text,
               std::size_t& position,
               ValueKind& kind) {
  skipWhitespace(text, position);
  if (position >= text.size()) return false;
  if (text[position] == '"') {
    kind = ValueKind::String;
    return skipString(text, position);
  }

  if (text[position] == '{' || text[position] == '[') {
    const char open = text[position];
    const char close = open == '{' ? '}' : ']';
    kind = open == '{' ? ValueKind::Object : ValueKind::Array;
    int depth = 0;
    bool inString = false;
    bool escaped = false;
    for (; position < text.size(); ++position) {
      const char ch = text[position];
      if (inString) {
        if (escaped) {
          escaped = false;
        } else if (ch == '\\') {
          escaped = true;
        } else if (ch == '"') {
          inString = false;
        }
        continue;
      }
      if (ch == '"') {
        inString = true;
      } else if (ch == open) {
        ++depth;
      } else if (ch == close) {
        --depth;
        if (depth == 0) {
          ++position;
          return true;
        }
      }
    }
    return false;
  }

  kind = ValueKind::Primitive;
  const std::size_t begin = position;
  while (position < text.size() && text[position] != ',' &&
         text[position] != '}' && text[position] != ']') {
    ++position;
  }
  std::size_t end = position;
  while (end > begin &&
         std::isspace(static_cast<unsigned char>(text[end - 1])) != 0) {
    --end;
  }
  return end > begin;
}

bool parseMember(std::string_view object,
                 std::size_t& position,
                 std::string_view& key,
                 ValueView& value) {
  skipWhitespace(object, position);
  if (position >= object.size() || object[position] != '"') return false;
  const std::size_t keyStart = position + 1;
  if (!skipString(object, position)) return false;
  const std::size_t keyEnd = position - 1;
  key = object.substr(keyStart, keyEnd - keyStart);

  skipWhitespace(object, position);
  if (position >= object.size() || object[position++] != ':') return false;
  skipWhitespace(object, position);
  const std::size_t valueStart = position;
  ValueKind kind = ValueKind::Invalid;
  if (!skipValue(object, position, kind)) return false;
  std::size_t valueEnd = position;
  while (valueEnd > valueStart &&
         std::isspace(static_cast<unsigned char>(object[valueEnd - 1])) != 0) {
    --valueEnd;
  }
  value = {object.substr(valueStart, valueEnd - valueStart), kind};
  return true;
}

}  // namespace

std::string_view ValueView::stringValue() const {
  if (kind != ValueKind::String || raw.size() < 2 || raw.front() != '"' ||
      raw.back() != '"') {
    return {};
  }
  return raw.substr(1, raw.size() - 2);
}

bool isValidObject(std::string_view json) {
  std::size_t position = 0;
  skipWhitespace(json, position);
  if (position >= json.size() || json[position++] != '{') return false;
  skipWhitespace(json, position);
  if (position < json.size() && json[position] == '}') {
    ++position;
    skipWhitespace(json, position);
    return position == json.size();
  }

  while (position < json.size()) {
    std::string_view key;
    ValueView value;
    if (!parseMember(json, position, key, value)) return false;
    skipWhitespace(json, position);
    if (position >= json.size()) return false;
    if (json[position] == ',') {
      ++position;
      continue;
    }
    if (json[position] == '}') {
      ++position;
      skipWhitespace(json, position);
      return position == json.size();
    }
    return false;
  }
  return false;
}

bool isValidArray(std::string_view json) {
  std::size_t cursor = 0;
  skipWhitespace(json, cursor);
  if (cursor >= json.size() || json[cursor] != '[') return false;
  ++cursor;
  skipWhitespace(json, cursor);
  if (cursor < json.size() && json[cursor] == ']') {
    ++cursor;
    skipWhitespace(json, cursor);
    return cursor == json.size();
  }
  while (cursor < json.size()) {
    ValueKind kind = ValueKind::Invalid;
    if (!skipValue(json, cursor, kind)) return false;
    skipWhitespace(json, cursor);
    if (cursor >= json.size()) return false;
    if (json[cursor] == ',') {
      ++cursor;
      continue;
    }
    if (json[cursor] == ']') {
      ++cursor;
      skipWhitespace(json, cursor);
      return cursor == json.size();
    }
    return false;
  }
  return false;
}

bool nextArrayValue(std::string_view array,
                    std::size_t& cursor,
                    ValueView& value) {
  if (cursor == 0) {
    skipWhitespace(array, cursor);
    if (cursor >= array.size() || array[cursor++] != '[') return false;
  }
  skipWhitespace(array, cursor);
  if (cursor >= array.size() || array[cursor] == ']') return false;
  const std::size_t begin = cursor;
  ValueKind kind = ValueKind::Invalid;
  if (!skipValue(array, cursor, kind)) return false;
  value = {array.substr(begin, cursor - begin), kind};
  skipWhitespace(array, cursor);
  if (cursor < array.size() && array[cursor] == ',') ++cursor;
  return true;
}

bool findMember(std::string_view object,
                std::string_view wanted,
                ValueView& value) {
  if (!isValidObject(object)) return false;
  std::size_t position = 0;
  skipWhitespace(object, position);
  ++position;
  skipWhitespace(object, position);
  if (position < object.size() && object[position] == '}') return false;

  while (position < object.size()) {
    std::string_view key;
    ValueView candidate;
    if (!parseMember(object, position, key, candidate)) return false;
    if (key == wanted) {
      value = candidate;
      return true;
    }
    skipWhitespace(object, position);
    if (position < object.size() && object[position] == ',') {
      ++position;
      continue;
    }
    return false;
  }
  return false;
}

}  // namespace smv::jsonlite
