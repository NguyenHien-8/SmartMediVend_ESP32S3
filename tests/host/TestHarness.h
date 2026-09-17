#pragma once

#include <exception>
#include <functional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace smv::test {

struct TestCase {
  const char* name;
  void (*function)();
};

inline std::vector<TestCase>& registry() {
  static std::vector<TestCase> tests;
  return tests;
}

class Registrar {
 public:
  Registrar(const char* name, void (*function)()) {
    registry().push_back({name, function});
  }
};

inline void require(bool condition,
                    const char* expression,
                    const char* file,
                    int line) {
  if (condition) {
    return;
  }
  std::ostringstream message;
  message << file << ':' << line << ": requirement failed: " << expression;
  throw std::runtime_error(message.str());
}

}  // namespace smv::test

#define SMV_JOIN_IMPL(a, b) a##b
#define SMV_JOIN(a, b) SMV_JOIN_IMPL(a, b)
#define TEST_CASE(name)                                                        \
  static void SMV_JOIN(test_, __LINE__)();                                    \
  static ::smv::test::Registrar SMV_JOIN(registrar_, __LINE__)(               \
      name, &SMV_JOIN(test_, __LINE__));                                      \
  static void SMV_JOIN(test_, __LINE__)()
#define REQUIRE(expression)                                                    \
  ::smv::test::require(static_cast<bool>(expression), #expression, __FILE__,  \
                       __LINE__)
#define REQUIRE_FALSE(expression) REQUIRE(!(expression))
