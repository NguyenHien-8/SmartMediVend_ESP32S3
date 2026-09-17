#include "TestHarness.h"

#include <string>

#include "SessionFixtures.h"
#include "src/medical/SafetyPolicy.h"
#include "src/medical/StructuredInputValidator.h"

using smv::medical::DangerSign;
using smv::medical::SafetyDecision;
using smv::medical::SafetyPolicy;
using smv::medical::StructuredInputValidator;
using smv::medical::ValidationError;

TEST_CASE("global safety policy denies unsupported and dangerous sessions") {
  auto underAge = smv::test::validSession();
  underAge.ageYears = 15;
  REQUIRE(SafetyPolicy{}.evaluate(underAge).decision == SafetyDecision::Deny);

  auto pregnant = smv::test::validSession();
  pregnant.pregnancyOrBreastfeeding = true;
  REQUIRE(SafetyPolicy{}.evaluate(pregnant).decision == SafetyDecision::Deny);

  auto dangerous = smv::test::validSession();
  dangerous.addDangerSign(DangerSign::DifficultyBreathing);
  REQUIRE(SafetyPolicy{}.evaluate(dangerous).decision == SafetyDecision::Deny);
}

TEST_CASE("global safety policy requests every missing required field") {
  auto ageMissing = smv::test::validSession();
  ageMissing.ageKnown = false;
  REQUIRE(SafetyPolicy{}.evaluate(ageMissing).decision ==
          SafetyDecision::NeedMoreInfo);

  auto weightMissing = smv::test::validSession();
  weightMissing.weightKnown = false;
  REQUIRE(SafetyPolicy{}.evaluate(weightMissing).decision ==
          SafetyDecision::NeedMoreInfo);

  auto medicinesMissing = smv::test::validSession();
  medicinesMissing.currentMedicinesKnown = false;
  REQUIRE(SafetyPolicy{}.evaluate(medicinesMissing).decision ==
          SafetyDecision::NeedMoreInfo);
}

TEST_CASE("globally complete mild session is eligible for medicine rules") {
  REQUIRE(SafetyPolicy{}.evaluate(smv::test::validSession()).decision ==
          SafetyDecision::Offer);
}

TEST_CASE("AI authority fields and oversized payloads are rejected") {
  const std::string forbidden =
      R"({"session_id":"s","turn_id":1,"age_years":20,"sku":"SMV-PARA500","relay":0,"vend":true})";
  REQUIRE(StructuredInputValidator{}.validateEnvelope(forbidden).error ==
          ValidationError::ForbiddenAuthorityField);

  const std::string oversized(4097, 'x');
  REQUIRE(StructuredInputValidator{}.validateEnvelope(oversized).error ==
          ValidationError::TooLarge);
}

TEST_CASE("malformed or missing session envelope is rejected") {
  REQUIRE(StructuredInputValidator{}.validateEnvelope("not-json").error ==
          ValidationError::MalformedJson);
  REQUIRE(StructuredInputValidator{}.validateEnvelope(R"({"turn_id":1})").error ==
          ValidationError::MissingSessionId);
}
