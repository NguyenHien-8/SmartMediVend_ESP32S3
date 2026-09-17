#include "SafetyPolicy.h"

namespace smv::medical {

SafetyResult SafetyPolicy::evaluate(const SymptomSession& session) const {
  if (!session.ageKnown) {
    return {SafetyDecision::NeedMoreInfo, SafetyReason::MissingAge};
  }
  if (session.ageYears > 120) {
    return {SafetyDecision::NeedMoreInfo, SafetyReason::ImplausibleAge};
  }
  if (session.ageYears < 16) {
    return {SafetyDecision::Deny, SafetyReason::UnsupportedAge};
  }
  if (!session.weightKnown) {
    return {SafetyDecision::NeedMoreInfo, SafetyReason::MissingWeight};
  }
  if (session.weightKg < 25 || session.weightKg > 300) {
    return {SafetyDecision::NeedMoreInfo, SafetyReason::ImplausibleWeight};
  }
  if (!session.pregnancyStatusKnown) {
    return {SafetyDecision::NeedMoreInfo, SafetyReason::MissingPregnancyStatus};
  }
  if (session.pregnancyOrBreastfeeding) {
    return {SafetyDecision::Deny, SafetyReason::PregnancyOrBreastfeeding};
  }
  if (!session.symptomsKnown) {
    return {SafetyDecision::NeedMoreInfo, SafetyReason::MissingSymptoms};
  }
  if (session.symptoms.empty()) {
    return {SafetyDecision::NeedMoreInfo, SafetyReason::NoSymptoms};
  }
  if (!session.durationKnown) {
    return {SafetyDecision::NeedMoreInfo, SafetyReason::MissingDuration};
  }
  if (!session.dangerSignsKnown) {
    return {SafetyDecision::NeedMoreInfo, SafetyReason::MissingDangerSigns};
  }
  if (!session.dangerSigns.empty()) {
    return {SafetyDecision::Deny, SafetyReason::DangerSignPresent};
  }
  if (!session.conditionsKnown) {
    return {SafetyDecision::NeedMoreInfo, SafetyReason::MissingConditions};
  }
  if (!session.currentMedicinesKnown) {
    return {SafetyDecision::NeedMoreInfo,
            SafetyReason::MissingCurrentMedicines};
  }
  if (!session.drugAllergiesKnown) {
    return {SafetyDecision::NeedMoreInfo, SafetyReason::MissingDrugAllergies};
  }
  return {SafetyDecision::Offer, SafetyReason::None};
}

}  // namespace smv::medical
