#pragma once

#include <cstdint>

#include "SymptomSession.h"

namespace smv::medical {

enum class SafetyDecision : uint8_t {
  NeedMoreInfo = 0,
  Deny,
  Refer,
  Offer
};

enum class SafetyReason : uint8_t {
  None = 0,
  MissingAge,
  MissingWeight,
  MissingPregnancyStatus,
  MissingSymptoms,
  MissingDuration,
  MissingDangerSigns,
  MissingConditions,
  MissingCurrentMedicines,
  MissingDrugAllergies,
  UnsupportedAge,
  ImplausibleAge,
  ImplausibleWeight,
  PregnancyOrBreastfeeding,
  DangerSignPresent,
  NoSymptoms
};

struct SafetyResult {
  SafetyDecision decision = SafetyDecision::NeedMoreInfo;
  SafetyReason reason = SafetyReason::None;
};

class SafetyPolicy {
 public:
  SafetyResult evaluate(const SymptomSession& session) const;
};

}  // namespace smv::medical
