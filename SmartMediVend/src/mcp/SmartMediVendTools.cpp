#include "SmartMediVendTools.h"

#include "../medical/StructuredInputValidator.h"

namespace smv::mcp {

ToolExecution SmartMediVendTools::execute(std::string_view name,
                                          std::string_view arguments) {
  if (name == names()[0]) return {true, true, dataSource_.deviceStatusJson(), {}};
  if (name == names()[1]) return {true, true, dataSource_.inventoryJson(), {}};
  if (name == names()[2]) {
    return {true, true, dataSource_.medicineInfoJson(arguments), {}};
  }
  if (name == names()[3]) {
    const auto validation =
        medical::StructuredInputValidator{}.validateEnvelope(arguments);
    if (!validation.ok()) {
      std::string tag = "INVALID_SYMPTOM_DATA";
      if (validation.error == medical::ValidationError::ForbiddenAuthorityField) {
        tag = "FORBIDDEN_AUTHORITY_FIELD";
      } else if (validation.error == medical::ValidationError::TooLarge) {
        tag = "ARGUMENTS_TOO_LARGE";
      } else if (validation.error == medical::ValidationError::MissingSessionId) {
        tag = "MISSING_SESSION_ID";
      }
      return {true, false, {}, std::move(tag)};
    }
    return {true, true, dataSource_.submitSymptomDataJson(arguments), {}};
  }
  if (name == names()[4]) {
    return {true, true, dataSource_.candidateStatusJson(), {}};
  }
  return {};
}

}  // namespace smv::mcp
