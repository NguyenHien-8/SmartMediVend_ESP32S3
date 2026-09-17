#include "TestHarness.h"

#include <array>
#include <string_view>

#include "SessionFixtures.h"
#include "src/medical/MedicalRuleEngine.h"

using smv::medical::Condition;
using smv::medical::MedicalRuleEngine;
using smv::medical::MedicineGroup;
using smv::medical::SafetyDecision;
using smv::medical::Symptom;
using smv::medical::SymptomSession;

namespace {

SymptomSession sessionWithOnly(Symptom symptom) {
  auto session = smv::test::validSession();
  session.symptoms.remove(Symptom::DryCough);
  session.addSymptom(symptom);
  return session;
}

smv::medical::CandidateResult evaluate(const SymptomSession& session) {
  return MedicalRuleEngine{}.evaluate(session);
}

}  // namespace

TEST_CASE("each supported mild symptom maps to its local medicine") {
  struct Case {
    Symptom symptom;
    std::string_view canonicalId;
  };
  constexpr std::array<Case, 13> cases{{
      {Symptom::MildHeadache, "PARACETAMOL_500"},
      {Symptom::MildInflammatoryPain, "IBUPROFEN_200"},
      {Symptom::AllergicRhinitis, "LORATADINE_10"},
      {Symptom::DryCough, "DEXTROMETHORPHAN_15"},
      {Symptom::ProductiveCough, "AMBROXOL_30"},
      {Symptom::MildSoreThroat, "DEQUALINIUM_025"},
      {Symptom::GasBloating, "SIMETHICONE_80"},
      {Symptom::AcidIndigestion, "ANTACID_200_200"},
      {Symptom::ShortTermReflux, "OMEPRAZOLE_10"},
      {Symptom::AcuteWateryDiarrhoea, "LOPERAMIDE_2"},
      {Symptom::ShortTermConstipation, "BISACODYL_5"},
      {Symptom::MotionSickness, "DIMENHYDRINATE_50"},
      {Symptom::DigestiveSupport, "S_BOULARDII_250"},
  }};

  for (const auto& item : cases) {
    auto session = sessionWithOnly(item.symptom);
    session.diarrhoeaAfterAntibioticsKnown = true;
    session.diarrhoeaAfterAntibiotics = false;
    session.constipationLifestyleTriedKnown = true;
    session.constipationLifestyleTried = true;
    session.alcoholUseKnown = true;
    session.alcoholUse = false;
    session.drivingStatusKnown = true;
    session.drivingOrOperatingMachinery = false;
    const auto result = evaluate(session);
    REQUIRE(result.decision == SafetyDecision::Offer);
    REQUIRE(result.has(item.canonicalId));
  }
}

TEST_CASE("paracetamol refers below 50kg and denies duplicate ingredient") {
  auto lowWeight = sessionWithOnly(Symptom::MildHeadache);
  lowWeight.weightKg = 49;
  REQUIRE(evaluate(lowWeight).decision == SafetyDecision::Refer);

  auto duplicate = sessionWithOnly(Symptom::MildHeadache);
  duplicate.addMedicine(MedicineGroup::ContainsParacetamol);
  REQUIRE(evaluate(duplicate).decision == SafetyDecision::Deny);
}

TEST_CASE("medicine-specific contraindications prevent offers") {
  auto ibuprofen = sessionWithOnly(Symptom::MildInflammatoryPain);
  ibuprofen.addCondition(Condition::PepticUlcer);
  REQUIRE(evaluate(ibuprofen).decision == SafetyDecision::Deny);

  auto loratadine = sessionWithOnly(Symptom::AllergicRhinitis);
  loratadine.addMedicine(MedicineGroup::OtherAntihistamine);
  REQUIRE(evaluate(loratadine).decision == SafetyDecision::Deny);

  auto dextromethorphan = sessionWithOnly(Symptom::DryCough);
  dextromethorphan.addMedicine(MedicineGroup::MaoiWithin14Days);
  REQUIRE(evaluate(dextromethorphan).decision == SafetyDecision::Deny);

  auto ambroxol = sessionWithOnly(Symptom::ProductiveCough);
  ambroxol.addCondition(Condition::SeriousSkinOrMucosalReaction);
  REQUIRE(evaluate(ambroxol).decision == SafetyDecision::Deny);

  auto simethicone = sessionWithOnly(Symptom::GasBloating);
  simethicone.addMedicine(MedicineGroup::Levothyroxine);
  REQUIRE(evaluate(simethicone).decision == SafetyDecision::Refer);

  auto antacid = sessionWithOnly(Symptom::AcidIndigestion);
  antacid.addMedicine(MedicineGroup::MedicineRequiringAntacidSpacing);
  REQUIRE(evaluate(antacid).decision == SafetyDecision::Refer);
}

TEST_CASE("age and gastrointestinal restrictions fail closed") {
  auto omeprazole = sessionWithOnly(Symptom::ShortTermReflux);
  omeprazole.ageYears = 17;
  REQUIRE(evaluate(omeprazole).decision == SafetyDecision::Refer);

  auto bisacodyl = sessionWithOnly(Symptom::ShortTermConstipation);
  bisacodyl.ageYears = 16;
  bisacodyl.constipationLifestyleTriedKnown = true;
  bisacodyl.constipationLifestyleTried = true;
  REQUIRE(evaluate(bisacodyl).decision == SafetyDecision::Refer);

  auto loperamide = sessionWithOnly(Symptom::AcuteWateryDiarrhoea);
  loperamide.diarrhoeaAfterAntibioticsKnown = true;
  loperamide.diarrhoeaAfterAntibiotics = true;
  REQUIRE(evaluate(loperamide).decision == SafetyDecision::Deny);

  auto longDiarrhoea = sessionWithOnly(Symptom::AcuteWateryDiarrhoea);
  longDiarrhoea.diarrhoeaAfterAntibioticsKnown = true;
  longDiarrhoea.durationHours = 49;
  REQUIRE(evaluate(longDiarrhoea).decision == SafetyDecision::Deny);
}

TEST_CASE("motion sickness and probiotic exclusions are enforced") {
  auto motion = sessionWithOnly(Symptom::MotionSickness);
  motion.alcoholUseKnown = true;
  motion.alcoholUse = false;
  motion.drivingStatusKnown = true;
  motion.drivingOrOperatingMachinery = true;
  REQUIRE(evaluate(motion).decision == SafetyDecision::Deny);

  auto probiotic = sessionWithOnly(Symptom::DigestiveSupport);
  probiotic.addCondition(Condition::Immunocompromised);
  REQUIRE(evaluate(probiotic).decision == SafetyDecision::Deny);
}

TEST_CASE("conditional screening questions remain required") {
  auto diarrhoea = sessionWithOnly(Symptom::AcuteWateryDiarrhoea);
  REQUIRE(evaluate(diarrhoea).decision == SafetyDecision::NeedMoreInfo);

  auto constipation = sessionWithOnly(Symptom::ShortTermConstipation);
  REQUIRE(evaluate(constipation).decision == SafetyDecision::NeedMoreInfo);

  auto motion = sessionWithOnly(Symptom::MotionSickness);
  REQUIRE(evaluate(motion).decision == SafetyDecision::NeedMoreInfo);
}

TEST_CASE("conflicting cough and acid options never create unsafe combinations") {
  auto cough = sessionWithOnly(Symptom::DryCough);
  cough.addSymptom(Symptom::ProductiveCough);
  REQUIRE(evaluate(cough).decision == SafetyDecision::NeedMoreInfo);

  auto acid = sessionWithOnly(Symptom::AcidIndigestion);
  acid.addSymptom(Symptom::ShortTermReflux);
  const auto acidResult = evaluate(acid);
  REQUIRE(acidResult.decision == SafetyDecision::Offer);
  REQUIRE(acidResult.count == 1);
  REQUIRE(acidResult.has("ANTACID_200_200"));
}

TEST_CASE("candidate list is deterministic and capped at three medicines") {
  auto session = sessionWithOnly(Symptom::MildHeadache);
  session.addSymptom(Symptom::AllergicRhinitis);
  session.addSymptom(Symptom::MildSoreThroat);
  session.addSymptom(Symptom::GasBloating);

  const auto result = evaluate(session);
  REQUIRE(result.decision == SafetyDecision::Offer);
  REQUIRE(result.count == 3);
  REQUIRE(result.canonicalIds[0] == "PARACETAMOL_500");
  REQUIRE(result.canonicalIds[1] == "LORATADINE_10");
  REQUIRE(result.canonicalIds[2] == "DEQUALINIUM_025");
}
