cmake_minimum_required(VERSION 3.20)
project(smv_host_tests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

enable_testing()

add_executable(smv_host_tests
  test_main.cpp
  test_app_types.cpp
)

target_include_directories(smv_host_tests PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/../../SmartMediVend
)

if(MSVC)
  target_compile_options(smv_host_tests PRIVATE /W4 /WX)
else()
  target_compile_options(smv_host_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

add_test(NAME smv_host_tests COMMAND smv_host_tests)
