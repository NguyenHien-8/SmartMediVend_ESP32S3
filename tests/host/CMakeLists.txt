cmake_minimum_required(VERSION 3.20)
project(smv_host_tests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

enable_testing()

add_executable(smv_host_tests
  test_main.cpp
  test_app_types.cpp
  test_catalog.cpp
  test_input_safety.cpp
  test_medical_rules.cpp
  test_inventory.cpp
  test_confirmation_vending.cpp
  test_protocol_mcp.cpp
  test_button.cpp
  test_bootstrap_session.cpp
  ../../SmartMediVend/src/medical/MedicineCatalog.cpp
  ../../SmartMediVend/src/medical/PharmacistGate.cpp
  ../../SmartMediVend/src/medical/StructuredInputValidator.cpp
  ../../SmartMediVend/src/medical/SafetyPolicy.cpp
  ../../SmartMediVend/src/medical/MedicineRules.cpp
  ../../SmartMediVend/src/medical/MedicalRuleEngine.cpp
  ../../SmartMediVend/src/inventory/InventoryManager.cpp
  ../../SmartMediVend/src/storage/SettingsStore.cpp
  ../../SmartMediVend/src/vending/ConfirmationGate.cpp
  ../../SmartMediVend/src/vending/VendGuard.cpp
  ../../SmartMediVend/src/vending/RelayDriver.cpp
  ../../SmartMediVend/src/vending/VendingManager.cpp
  ../../SmartMediVend/src/core/JsonLite.cpp
  ../../SmartMediVend/src/protocol/XiaozhiProtocol.cpp
  ../../SmartMediVend/src/mcp/McpServer.cpp
  ../../SmartMediVend/src/mcp/SmartMediVendTools.cpp
  ../../SmartMediVend/src/app/ButtonController.cpp
  ../../SmartMediVend/src/network/XiaozhiBootstrapClient.cpp
  ../../SmartMediVend/src/network/XiaozhiSession.cpp
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
