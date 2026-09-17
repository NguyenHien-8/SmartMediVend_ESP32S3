#pragma once

#include <array>
#include <cstddef>
#include <string_view>

#include "MedicineRules.h"
#include "SafetyPolicy.h"

namespace smv::medical {

struct CandidateResult {
  SafetyDecision decision = SafetyDecision::NeedMoreInfo;
  RuleReason reason = RuleReason::None;
  std::array<std::string_view, 3> canonicalIds{};
  std::size_t count = 0;

  bool has(std::string_view canonicalId) const;
  bool offered() const {
    return decision == SafetyDecision::Offer && count > 0;
  }
};

class MedicalRuleEngine {
 public:
  CandidateResult evaluate(const SymptomSession& session) const;

 private:
  SafetyPolicy safetyPolicy_;
  MedicineRules medicineRules_;
};

}  // namespace smv::medical
