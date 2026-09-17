#pragma once

#include <array>
#include <string>
#include <string_view>

namespace smv::mcp {

class IToolDataSource {
 public:
  virtual ~IToolDataSource() = default;
  virtual std::string deviceStatusJson() const = 0;
  virtual std::string inventoryJson() const = 0;
  virtual std::string medicineInfoJson(std::string_view arguments) const = 0;
  virtual std::string submitSymptomDataJson(std::string_view arguments) = 0;
  virtual std::string candidateStatusJson() const = 0;
};

struct ToolExecution {
  bool found = false;
  bool ok = false;
  std::string json;
  std::string errorTag;
};

class SmartMediVendTools {
 public:
  explicit SmartMediVendTools(IToolDataSource& dataSource)
      : dataSource_(dataSource) {}

  static constexpr std::array<std::string_view, 5> names() {
    return {"smartmedivend.get_device_status",
            "smartmedivend.get_inventory",
            "smartmedivend.get_medicine_info",
            "smartmedivend.submit_symptom_data",
            "smartmedivend.get_candidate_status"};
  }

  ToolExecution execute(std::string_view name,
                        std::string_view arguments);

 private:
  IToolDataSource& dataSource_;
};

}  // namespace smv::mcp
