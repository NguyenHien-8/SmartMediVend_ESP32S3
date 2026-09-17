#include "TestHarness.h"

#include "src/medical/MedicalRuleEngine.h"
#include "src/medical/SessionTurnGate.h"
#include "src/medical/SymptomJsonDecoder.h"

using smv::medical::MedicalRuleEngine;
using smv::medical::SessionTurnGate;
using smv::medical::SymptomDecodeError;
using smv::medical::SymptomJsonDecoder;

TEST_CASE("structured AI observations decode into a local rule session") {
  constexpr const char* json = R"({
    "session_id":"session-1","turn_id":4,"age_years":30,"weight_kg":60,
    "pregnancy_or_breastfeeding":false,"symptoms":["dry_cough"],
    "duration_hours":24,"danger_signs":[],"conditions":[],
    "current_medicines":[],"allergies":[],"alcohol_use":false,
    "driving_or_operating_machinery":false,
    "diarrhoea_after_antibiotics":false,
    "constipation_lifestyle_tried":true
  })";
  const auto decoded = SymptomJsonDecoder::decode(json);
  REQUIRE(decoded.error == SymptomDecodeError::None);
  REQUIRE(decoded.session.sessionId == "session-1");
  const auto candidate = MedicalRuleEngine{}.evaluate(decoded.session);
  REQUIRE(candidate.offered());
  REQUIRE(candidate.has("DEXTROMETHORPHAN_15"));
}

TEST_CASE("unknown enum or wrong primitive fails closed") {
  REQUIRE(SymptomJsonDecoder::decode(
              R"({"session_id":"s","turn_id":1,"symptoms":["invented"]})")
              .error == SymptomDecodeError::UnknownEnumValue);
  REQUIRE(SymptomJsonDecoder::decode(
              R"({"session_id":"s","turn_id":1,"age_years":"30"})")
              .error == SymptomDecodeError::WrongType);
}

TEST_CASE("turn id is required and stale or mismatched turns fail closed") {
  REQUIRE(SymptomJsonDecoder::decode(R"({"session_id":"s"})").error ==
          SymptomDecodeError::MissingTurnId);

  SessionTurnGate gate;
  REQUIRE(gate.accept("session-a", "session-a", 4));
  REQUIRE_FALSE(gate.accept("session-a", "session-a", 4));
  REQUIRE_FALSE(gate.accept("session-a", "session-a", 3));
  REQUIRE_FALSE(gate.accept("session-a", "session-b", 5));
  gate.reset();
  REQUIRE(gate.accept("session-b", "session-b", 1));
}
