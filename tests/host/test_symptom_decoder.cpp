#include "TestHarness.h"

#include "src/medical/MedicalRuleEngine.h"
#include "src/medical/SymptomJsonDecoder.h"

using smv::medical::MedicalRuleEngine;
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
              R"({"session_id":"s","symptoms":["invented"]})")
              .error == SymptomDecodeError::UnknownEnumValue);
  REQUIRE(SymptomJsonDecoder::decode(
              R"({"session_id":"s","age_years":"30"})")
              .error == SymptomDecodeError::WrongType);
}
