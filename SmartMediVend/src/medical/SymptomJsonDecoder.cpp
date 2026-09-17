#include "SymptomJsonDecoder.h"

#include <array>
#include <charconv>
#include <utility>

#include "../core/JsonLite.h"
#include "StructuredInputValidator.h"

namespace smv::medical {
namespace {

using jsonlite::ValueKind;
using jsonlite::ValueView;

template <typename Integer>
bool parseInteger(const ValueView& value, Integer& result) {
  if (value.kind != ValueKind::Primitive || value.raw.empty()) return false;
  const char* begin = value.raw.data();
  const auto parsed = std::from_chars(begin, begin + value.raw.size(), result);
  return parsed.ec == std::errc{} &&
         parsed.ptr == begin + value.raw.size();
}

bool parseBoolean(const ValueView& value, bool& result) {
  if (value.kind != ValueKind::Primitive) return false;
  if (value.raw == "true") {
    result = true;
    return true;
  }
  if (value.raw == "false") {
    result = false;
    return true;
  }
  return false;
}

template <typename Integer>
bool optionalInteger(std::string_view json,
                     std::string_view key,
                     Integer minimum,
                     Integer maximum,
                     Integer& destination,
                     bool& known,
                     SymptomDecodeError& error) {
  ValueView value;
  if (!jsonlite::findMember(json, key, value)) return true;
  Integer parsed{};
  if (!parseInteger(value, parsed)) {
    error = SymptomDecodeError::WrongType;
    return false;
  }
  if (parsed < minimum || parsed > maximum) {
    error = SymptomDecodeError::OutOfRange;
    return false;
  }
  destination = parsed;
  known = true;
  return true;
}

bool optionalBoolean(std::string_view json,
                     std::string_view key,
                     bool& destination,
                     bool& known,
                     SymptomDecodeError& error) {
  ValueView value;
  if (!jsonlite::findMember(json, key, value)) return true;
  if (!parseBoolean(value, destination)) {
    error = SymptomDecodeError::WrongType;
    return false;
  }
  known = true;
  return true;
}

template <typename Enum, std::size_t N, typename Add>
bool decodeEnumArray(std::string_view json,
                     std::string_view key,
                     const std::array<std::pair<std::string_view, Enum>, N>& map,
                     std::size_t maximum,
                     bool& known,
                     Add add,
                     SymptomDecodeError& error) {
  ValueView array;
  if (!jsonlite::findMember(json, key, array)) return true;
  if (array.kind != ValueKind::Array || !jsonlite::isValidArray(array.raw)) {
    error = SymptomDecodeError::WrongType;
    return false;
  }
  known = true;
  std::size_t cursor = 0;
  std::size_t count = 0;
  ValueView item;
  while (jsonlite::nextArrayValue(array.raw, cursor, item)) {
    if (item.kind != ValueKind::String) {
      error = SymptomDecodeError::WrongType;
      return false;
    }
    if (++count > maximum) {
      error = SymptomDecodeError::TooManyValues;
      return false;
    }
    bool found = false;
    for (const auto& entry : map) {
      if (entry.first == item.stringValue()) {
        add(entry.second);
        found = true;
        break;
      }
    }
    if (!found) {
      error = SymptomDecodeError::UnknownEnumValue;
      return false;
    }
  }
  return true;
}

constexpr std::array<std::pair<std::string_view, Symptom>, 16> kSymptoms{{
    {"fever", Symptom::Fever}, {"mild_headache", Symptom::MildHeadache},
    {"mild_body_ache", Symptom::MildBodyAche},
    {"mild_inflammatory_pain", Symptom::MildInflammatoryPain},
    {"allergic_rhinitis", Symptom::AllergicRhinitis},
    {"dry_cough", Symptom::DryCough},
    {"productive_cough", Symptom::ProductiveCough},
    {"mild_sore_throat", Symptom::MildSoreThroat},
    {"gas_bloating", Symptom::GasBloating},
    {"acid_indigestion", Symptom::AcidIndigestion},
    {"short_term_reflux", Symptom::ShortTermReflux},
    {"acute_watery_diarrhoea", Symptom::AcuteWateryDiarrhoea},
    {"short_term_constipation", Symptom::ShortTermConstipation},
    {"motion_sickness", Symptom::MotionSickness},
    {"digestive_support", Symptom::DigestiveSupport},
    {"unexplained_nausea", Symptom::UnexplainedNausea},
}};

constexpr std::array<std::pair<std::string_view, DangerSign>, 19> kDanger{{
    {"difficulty_breathing", DangerSign::DifficultyBreathing},
    {"chest_pain", DangerSign::ChestPain},
    {"confusion", DangerSign::Confusion}, {"fainting", DangerSign::Fainting},
    {"seizure", DangerSign::Seizure},
    {"weakness_or_speech_problem", DangerSign::WeaknessOrSpeechProblem},
    {"sudden_severe_headache", DangerSign::SuddenSevereHeadache},
    {"stiff_neck", DangerSign::StiffNeck},
    {"coughing_blood", DangerSign::CoughingBlood},
    {"vomiting_blood", DangerSign::VomitingBlood},
    {"bloody_or_black_stool", DangerSign::BloodyOrBlackStool},
    {"severe_abdominal_pain", DangerSign::SevereAbdominalPain},
    {"dehydration", DangerSign::Dehydration},
    {"cannot_keep_fluids", DangerSign::CannotKeepFluids},
    {"difficulty_swallowing", DangerSign::DifficultySwallowing},
    {"drooling", DangerSign::Drooling}, {"stridor", DangerSign::Stridor},
    {"unintentional_weight_loss", DangerSign::UnintentionalWeightLoss},
    {"rapidly_worsening", DangerSign::RapidlyWorsening},
}};

constexpr std::array<std::pair<std::string_view, Condition>, 23> kConditions{{
    {"liver_disease", Condition::LiverDisease},
    {"kidney_disease", Condition::KidneyDisease},
    {"heart_disease", Condition::HeartDisease},
    {"peptic_ulcer", Condition::PepticUlcer},
    {"gastrointestinal_bleeding", Condition::GastrointestinalBleeding},
    {"nsaid_allergy", Condition::NsaidAllergy},
    {"nsaid_triggered_asthma", Condition::NsaidTriggeredAsthma},
    {"asthma_or_copd", Condition::AsthmaOrCopd},
    {"glaucoma", Condition::Glaucoma},
    {"urination_difficulty", Condition::UrinationDifficulty},
    {"immunocompromised", Condition::Immunocompromised},
    {"critically_ill", Condition::CriticallyIll},
    {"central_venous_catheter", Condition::CentralVenousCatheter},
    {"inflammatory_bowel_disease", Condition::InflammatoryBowelDisease},
    {"bowel_obstruction", Condition::BowelObstruction},
    {"thyroid_treatment", Condition::ThyroidTreatment},
    {"sodium_restriction", Condition::SodiumRestriction},
    {"chronic_cough", Condition::ChronicCough},
    {"constipation", Condition::Constipation},
    {"abdominal_swelling", Condition::AbdominalSwelling},
    {"yeast_allergy", Condition::YeastAllergy},
    {"serious_skin_or_mucosal_reaction",
     Condition::SeriousSkinOrMucosalReaction},
    {"heavy_alcohol_use", Condition::HeavyAlcoholUse},
}};

constexpr std::array<std::pair<std::string_view, MedicineGroup>, 13> kMedicines{{
    {"contains_paracetamol", MedicineGroup::ContainsParacetamol},
    {"anticoagulant", MedicineGroup::Anticoagulant},
    {"systemic_steroid", MedicineGroup::SystemicSteroid},
    {"other_nsaid", MedicineGroup::OtherNsaid},
    {"other_antihistamine", MedicineGroup::OtherAntihistamine},
    {"maoi_within_14_days", MedicineGroup::MaoiWithin14Days},
    {"sedative_or_tranquilizer", MedicineGroup::SedativeOrTranquilizer},
    {"levothyroxine", MedicineGroup::Levothyroxine},
    {"clopidogrel", MedicineGroup::Clopidogrel},
    {"important_ppi_interaction", MedicineGroup::ImportantPpiInteraction},
    {"recent_antacid", MedicineGroup::RecentAntacid},
    {"medicine_requiring_antacid_spacing",
     MedicineGroup::MedicineRequiringAntacidSpacing},
    {"antifungal", MedicineGroup::Antifungal},
}};

constexpr std::array<std::pair<std::string_view, DrugAllergy>, 13> kAllergies{{
    {"paracetamol", DrugAllergy::Paracetamol},
    {"ibuprofen_or_nsaid", DrugAllergy::IbuprofenOrNsaid},
    {"loratadine", DrugAllergy::Loratadine},
    {"dextromethorphan", DrugAllergy::Dextromethorphan},
    {"ambroxol", DrugAllergy::Ambroxol},
    {"dequalinium", DrugAllergy::Dequalinium},
    {"simethicone", DrugAllergy::Simethicone},
    {"aluminium_or_magnesium_antacid",
     DrugAllergy::AluminiumOrMagnesiumAntacid},
    {"omeprazole_or_ppi", DrugAllergy::OmeprazoleOrPpi},
    {"loperamide", DrugAllergy::Loperamide},
    {"bisacodyl", DrugAllergy::Bisacodyl},
    {"dimenhydrinate", DrugAllergy::Dimenhydrinate},
    {"yeast", DrugAllergy::Yeast},
}};

}  // namespace

SymptomDecodeResult SymptomJsonDecoder::decode(std::string_view json) {
  SymptomDecodeResult result;
  if (json.size() > StructuredInputValidator::kMaxEnvelopeBytes) {
    result.error = SymptomDecodeError::TooLarge;
    return result;
  }
  if (!jsonlite::isValidObject(json)) {
    result.error = SymptomDecodeError::MalformedJson;
    return result;
  }
  ValueView sessionId;
  if (!jsonlite::findMember(json, "session_id", sessionId) ||
      sessionId.kind != ValueKind::String || sessionId.stringValue().empty() ||
      sessionId.stringValue().size() > 128U) {
    result.error = SymptomDecodeError::MissingSessionId;
    return result;
  }
  result.session.sessionId.assign(sessionId.stringValue());

  bool turnKnown = false;
  if (!optionalInteger<uint32_t>(json, "turn_id", 0U, 1000000U,
                                 result.session.turnId, turnKnown,
                                 result.error) ||
      !optionalInteger<uint8_t>(json, "age_years", 0U, 120U,
                                result.session.ageYears,
                                result.session.ageKnown, result.error) ||
      !optionalInteger<uint16_t>(json, "weight_kg", 1U, 300U,
                                 result.session.weightKg,
                                 result.session.weightKnown, result.error) ||
      !optionalInteger<uint32_t>(json, "duration_hours", 0U, 87600U,
                                 result.session.durationHours,
                                 result.session.durationKnown, result.error) ||
      !optionalBoolean(json, "pregnancy_or_breastfeeding",
                       result.session.pregnancyOrBreastfeeding,
                       result.session.pregnancyStatusKnown, result.error) ||
      !optionalBoolean(json, "alcohol_use", result.session.alcoholUse,
                       result.session.alcoholUseKnown, result.error) ||
      !optionalBoolean(json, "driving_or_operating_machinery",
                       result.session.drivingOrOperatingMachinery,
                       result.session.drivingStatusKnown, result.error) ||
      !optionalBoolean(json, "diarrhoea_after_antibiotics",
                       result.session.diarrhoeaAfterAntibiotics,
                       result.session.diarrhoeaAfterAntibioticsKnown,
                       result.error) ||
      !optionalBoolean(json, "constipation_lifestyle_tried",
                       result.session.constipationLifestyleTried,
                       result.session.constipationLifestyleTriedKnown,
                       result.error)) {
    return result;
  }
  if (!turnKnown) {
    result.error = SymptomDecodeError::MissingTurnId;
    return result;
  }

  if (!decodeEnumArray(json, "symptoms", kSymptoms, 8,
                       result.session.symptomsKnown,
                       [&](Symptom value) { result.session.addSymptom(value); },
                       result.error) ||
      !decodeEnumArray(json, "danger_signs", kDanger, 16,
                       result.session.dangerSignsKnown,
                       [&](DangerSign value) {
                         result.session.addDangerSign(value);
                       },
                       result.error) ||
      !decodeEnumArray(json, "conditions", kConditions, 12,
                       result.session.conditionsKnown,
                       [&](Condition value) {
                         result.session.addCondition(value);
                       },
                       result.error) ||
      !decodeEnumArray(json, "current_medicines", kMedicines, 16,
                       result.session.currentMedicinesKnown,
                       [&](MedicineGroup value) {
                         result.session.addMedicine(value);
                       },
                       result.error) ||
      !decodeEnumArray(json, "allergies", kAllergies, 8,
                       result.session.drugAllergiesKnown,
                       [&](DrugAllergy value) {
                         result.session.addAllergy(value);
                       },
                       result.error)) {
    return result;
  }
  return result;
}

}  // namespace smv::medical
