#include "TestHarness.h"

#include <iostream>
#include <string>

int main(int argc, char** argv) {
  const std::string filter = argc > 1 ? argv[1] : "";
  int executed = 0;
  int failed = 0;

  for (const auto& test : smv::test::registry()) {
    if (!filter.empty() && std::string(test.name).find(filter) == std::string::npos) {
      continue;
    }

    ++executed;
    try {
      test.function();
      std::cout << "[PASS] " << test.name << '\n';
    } catch (const std::exception& error) {
      ++failed;
      std::cerr << "[FAIL] " << test.name << " - " << error.what() << '\n';
    } catch (...) {
      ++failed;
      std::cerr << "[FAIL] " << test.name << " - unknown exception\n";
    }
  }

  std::cout << "Executed " << executed << ", failures " << failed << '\n';
  return failed == 0 ? 0 : 1;
}
