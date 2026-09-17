#pragma once

#include <bitset>
#include <cstddef>
#include <cstdint>
#include <string>

namespace smv::medical {

enum class Symptom : uint8_t {
  Fever = 0,
  MildHeadache,
  MildBodyAche,
  MildInflammatoryPain,
  AllergicRhinitis,
  DryCough,
  ProductiveCough,
  MildSoreThroat,
  GasBloating,
  AcidIndigestion,
  ShortTermReflux,
  AcuteWateryDiarrhoea,
  ShortTermConstipation,
  MotionSickness,
  DigestiveSupport,
  UnexplainedNausea,
  Count
};

enum class DangerSign : uint8_t {
  DifficultyBreathing = 0,
  ChestPain,
  Confusion,
  Fainting,
  Seizure,
  WeaknessOrSpeechProblem,
  SuddenSevereHeadache,
  StiffNeck,
  CoughingBlood,
  VomitingBlood,
  BloodyOrBlackStool,
  SevereAbdominalPain,
  Dehydration,
  CannotKeepFluids,
  DifficultySwallowing,
  Drooling,
  Stridor,
  UnintentionalWeightLoss,
  RapidlyWorsening,
  Count
};

enum class Condition : uint8_t {
  LiverDisease = 0,
  KidneyDisease,
  HeartDisease,
  PepticUlcer,
  GastrointestinalBleeding,
  NsaidAllergy,
  NsaidTriggeredAsthma,
  AsthmaOrCopd,
  Glaucoma,
  UrinationDifficulty,
  Immunocompromised,
  CriticallyIll,
  CentralVenousCatheter,
  InflammatoryBowelDisease,
  BowelObstruction,
  ThyroidTreatment,
  SodiumRestriction,
  ChronicCough,
  Constipation,
  AbdominalSwelling,
  YeastAllergy,
  SeriousSkinOrMucosalReaction,
  HeavyAlcoholUse,
  Count
};

enum class MedicineGroup : uint8_t {
  ContainsParacetamol = 0,
  Anticoagulant,
  SystemicSteroid,
  OtherNsaid,
  OtherAntihistamine,
  MaoiWithin14Days,
  SedativeOrTranquilizer,
  Levothyroxine,
  Clopidogrel,
  ImportantPpiInteraction,
  RecentAntacid,
  MedicineRequiringAntacidSpacing,
  Antifungal,
  Count
};

enum class DrugAllergy : uint8_t {
  Paracetamol = 0,
  IbuprofenOrNsaid,
  Loratadine,
  Dextromethorphan,
  Ambroxol,
  Dequalinium,
  Simethicone,
  AluminiumOrMagnesiumAntacid,
  OmeprazoleOrPpi,
  Loperamide,
  Bisacodyl,
  Dimenhydrinate,
  Yeast,
  Count
};

template <typename Enum, std::size_t Size>
class EnumSet {
 public:
  void add(Enum value) { values_.set(static_cast<std::size_t>(value)); }
  void remove(Enum value) { values_.reset(static_cast<std::size_t>(value)); }
  bool contains(Enum value) const {
    return values_.test(static_cast<std::size_t>(value));
  }
  bool empty() const { return values_.none(); }
  std::size_t count() const { return values_.count(); }

 private:
  std::bitset<Size> values_;
};

struct SymptomSession {
  std::string sessionId;
  uint32_t turnId = 0;

  bool ageKnown = false;
  uint8_t ageYears = 0;
  bool weightKnown = false;
  uint16_t weightKg = 0;
  bool pregnancyStatusKnown = false;
  bool pregnancyOrBreastfeeding = false;
  bool symptomsKnown = false;
  bool durationKnown = false;
  uint32_t durationHours = 0;
  bool dangerSignsKnown = false;
  bool conditionsKnown = false;
  bool currentMedicinesKnown = false;
  bool drugAllergiesKnown = false;

  bool alcoholUseKnown = false;
  bool alcoholUse = false;
  bool drivingStatusKnown = false;
  bool drivingOrOperatingMachinery = false;
  bool diarrhoeaAfterAntibioticsKnown = false;
  bool diarrhoeaAfterAntibiotics = false;
  bool constipationLifestyleTriedKnown = false;
  bool constipationLifestyleTried = false;

  EnumSet<Symptom, static_cast<std::size_t>(Symptom::Count)> symptoms;
  EnumSet<DangerSign, static_cast<std::size_t>(DangerSign::Count)> dangerSigns;
  EnumSet<Condition, static_cast<std::size_t>(Condition::Count)> conditions;
  EnumSet<MedicineGroup, static_cast<std::size_t>(MedicineGroup::Count)>
      currentMedicines;
  EnumSet<DrugAllergy, static_cast<std::size_t>(DrugAllergy::Count)> allergies;

  void addSymptom(Symptom symptom) { symptoms.add(symptom); }
  bool hasSymptom(Symptom symptom) const { return symptoms.contains(symptom); }
  void addDangerSign(DangerSign sign) { dangerSigns.add(sign); }
  bool hasDangerSign(DangerSign sign) const { return dangerSigns.contains(sign); }
  void addCondition(Condition condition) { conditions.add(condition); }
  bool hasCondition(Condition condition) const {
    return conditions.contains(condition);
  }
  void addMedicine(MedicineGroup medicine) { currentMedicines.add(medicine); }
  bool usesMedicine(MedicineGroup medicine) const {
    return currentMedicines.contains(medicine);
  }
  void addAllergy(DrugAllergy allergy) { allergies.add(allergy); }
  bool hasAllergy(DrugAllergy allergy) const {
    return allergies.contains(allergy);
  }
};

}  // namespace smv::medical
