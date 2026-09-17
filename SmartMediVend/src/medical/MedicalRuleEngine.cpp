#include "MedicalRuleEngine.h"

#include <array>

namespace smv::medical {
namespace {

constexpr std::array<std::string_view, 13> kRuleOrder = {
    "PARACETAMOL_500",  "IBUPROFEN_200",       "LORATADINE_10",
    "DEXTROMETHORPHAN_15", "AMBROXOL_30",      "DEQUALINIUM_025",
    "SIMETHICONE_80",   "ANTACID_200_200",     "OMEPRAZOLE_10",
    "LOPERAMIDE_2",     "BISACODYL_5",         "DIMENHYDRINATE_50",
    "S_BOULARDII_250"};

bool contains(const CandidateResult& result, std::string_view id) {
  for (std::size_t i = 0; i < result.count; ++i) {
    if (result.canonicalIds[i] == id) return true;
  }
  return false;
}

}  // namespace

bool CandidateResult::has(std::string_view canonicalId) const {
  return contains(*this, canonicalId);
}

CandidateResult MedicalRuleEngine::evaluate(
    const SymptomSession& session) const {
  const auto global = safetyPolicy_.evaluate(session);
  if (global.decision != SafetyDecision::Offer) {
    return {global.decision, RuleReason::None, {}, 0};
  }

  if (session.hasSymptom(Symptom::DryCough) &&
      session.hasSymptom(Symptom::ProductiveCough)) {
    return {SafetyDecision::NeedMoreInfo, RuleReason::ConflictingSymptoms,
            {}, 0};
  }

  CandidateResult result;
  result.decision = SafetyDecision::Offer;

  for (const auto id : kRuleOrder) {
    const auto check = medicineRules_.evaluate(id, session);
    switch (check.disposition) {
      case RuleDisposition::NeedMoreInfo:
        return {SafetyDecision::NeedMoreInfo, check.reason, {}, 0};
      case RuleDisposition::Refer:
        return {SafetyDecision::Refer, check.reason, {}, 0};
      case RuleDisposition::Deny:
        return {SafetyDecision::Deny, check.reason, {}, 0};
      case RuleDisposition::Candidate:
        break;
      case RuleDisposition::NotApplicable:
      default:
        continue;
    }

    if (id == "OMEPRAZOLE_10" && contains(result, "ANTACID_200_200")) {
      continue;
    }
    if (id == "DIMENHYDRINATE_50" &&
        contains(result, "DEXTROMETHORPHAN_15")) {
      return {SafetyDecision::Refer, RuleReason::CombinationConflict, {}, 0};
    }
    if (!contains(result, id) && result.count < result.canonicalIds.size()) {
      result.canonicalIds[result.count++] = id;
    }
  }

  if (result.count == 0) {
    return {SafetyDecision::Deny, RuleReason::UnsupportedSymptom, {}, 0};
  }
  return result;
}

}  // namespace smv::medical
