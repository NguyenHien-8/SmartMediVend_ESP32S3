#pragma once

#include <cstdint>
#include <string_view>

#include "SymptomSession.h"

namespace smv::medical {

enum class RuleDisposition : uint8_t {
  NotApplicable = 0,
  Candidate,
  NeedMoreInfo,
  Refer,
  Deny
};

enum class RuleReason : uint8_t {
  None = 0,
  UnsupportedSymptom,
  Allergy,
  Contraindication,
  Interaction,
  DuplicateIngredient,
  WeightBelow50Kg,
  AgeRequiresPharmacist,
  MissingConditionalAnswer,
  DurationTooLong,
  ConflictingSymptoms,
  CombinationConflict,
  LifestyleMeasuresNotTried
};

struct MedicineRuleResult {
  RuleDisposition disposition = RuleDisposition::NotApplicable;
  RuleReason reason = RuleReason::None;
  std::string_view canonicalId;
};

class MedicineRules {
 public:
  MedicineRuleResult evaluate(std::string_view canonicalId,
                              const SymptomSession& session) const;
};

}  // namespace smv::medical
