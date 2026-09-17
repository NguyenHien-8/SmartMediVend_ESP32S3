#pragma once

#include "src/medical/SymptomSession.h"

namespace smv::test {

inline medical::SymptomSession validSession() {
  medical::SymptomSession session;
  session.sessionId = "session-1";
  session.turnId = 1;
  session.ageKnown = true;
  session.ageYears = 25;
  session.weightKnown = true;
  session.weightKg = 60;
  session.pregnancyStatusKnown = true;
  session.pregnancyOrBreastfeeding = false;
  session.symptomsKnown = true;
  session.addSymptom(medical::Symptom::DryCough);
  session.durationKnown = true;
  session.durationHours = 24;
  session.dangerSignsKnown = true;
  session.conditionsKnown = true;
  session.currentMedicinesKnown = true;
  session.drugAllergiesKnown = true;
  return session;
}

}  // namespace smv::test
