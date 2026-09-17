#include "MedicineRules.h"

namespace smv::medical {
namespace {

MedicineRuleResult candidate(std::string_view id) {
  return {RuleDisposition::Candidate, RuleReason::None, id};
}

MedicineRuleResult stop(RuleDisposition disposition,
                        RuleReason reason,
                        std::string_view id) {
  return {disposition, reason, id};
}

MedicineRuleResult notApplicable(std::string_view id) {
  return {RuleDisposition::NotApplicable, RuleReason::UnsupportedSymptom, id};
}

}  // namespace

MedicineRuleResult MedicineRules::evaluate(
    std::string_view id,
    const SymptomSession& s) const {
  if (id == "PARACETAMOL_500") {
    if (!(s.hasSymptom(Symptom::Fever) ||
          s.hasSymptom(Symptom::MildHeadache) ||
          s.hasSymptom(Symptom::MildBodyAche))) {
      return notApplicable(id);
    }
    if (s.hasAllergy(DrugAllergy::Paracetamol) ||
        s.hasCondition(Condition::LiverDisease) ||
        s.hasCondition(Condition::KidneyDisease) ||
        s.hasCondition(Condition::HeavyAlcoholUse)) {
      return stop(RuleDisposition::Deny, RuleReason::Contraindication, id);
    }
    if (s.usesMedicine(MedicineGroup::ContainsParacetamol)) {
      return stop(RuleDisposition::Deny, RuleReason::DuplicateIngredient, id);
    }
    if (s.weightKg < 50) {
      return stop(RuleDisposition::Refer, RuleReason::WeightBelow50Kg, id);
    }
    return candidate(id);
  }

  if (id == "IBUPROFEN_200") {
    if (!s.hasSymptom(Symptom::MildInflammatoryPain)) return notApplicable(id);
    if (s.hasAllergy(DrugAllergy::IbuprofenOrNsaid) ||
        s.hasCondition(Condition::PepticUlcer) ||
        s.hasCondition(Condition::GastrointestinalBleeding) ||
        s.hasCondition(Condition::KidneyDisease) ||
        s.hasCondition(Condition::HeartDisease) ||
        s.hasCondition(Condition::LiverDisease) ||
        s.hasCondition(Condition::NsaidAllergy) ||
        s.hasCondition(Condition::NsaidTriggeredAsthma) ||
        s.usesMedicine(MedicineGroup::Anticoagulant) ||
        s.usesMedicine(MedicineGroup::SystemicSteroid) ||
        s.usesMedicine(MedicineGroup::OtherNsaid)) {
      return stop(RuleDisposition::Deny, RuleReason::Contraindication, id);
    }
    return candidate(id);
  }

  if (id == "LORATADINE_10") {
    if (!s.hasSymptom(Symptom::AllergicRhinitis)) return notApplicable(id);
    if (s.hasAllergy(DrugAllergy::Loratadine) ||
        s.usesMedicine(MedicineGroup::OtherAntihistamine)) {
      return stop(RuleDisposition::Deny, RuleReason::Interaction, id);
    }
    if (s.hasCondition(Condition::LiverDisease)) {
      return stop(RuleDisposition::Refer, RuleReason::Contraindication, id);
    }
    return candidate(id);
  }

  if (id == "DEXTROMETHORPHAN_15") {
    if (!s.hasSymptom(Symptom::DryCough)) return notApplicable(id);
    if (s.hasAllergy(DrugAllergy::Dextromethorphan) ||
        s.hasCondition(Condition::ChronicCough) ||
        s.hasCondition(Condition::AsthmaOrCopd) ||
        s.usesMedicine(MedicineGroup::MaoiWithin14Days) ||
        s.usesMedicine(MedicineGroup::SedativeOrTranquilizer)) {
      return stop(RuleDisposition::Deny, RuleReason::Interaction, id);
    }
    return candidate(id);
  }

  if (id == "AMBROXOL_30") {
    if (!s.hasSymptom(Symptom::ProductiveCough)) return notApplicable(id);
    if (s.hasAllergy(DrugAllergy::Ambroxol) ||
        s.hasCondition(Condition::SeriousSkinOrMucosalReaction)) {
      return stop(RuleDisposition::Deny, RuleReason::Contraindication, id);
    }
    return candidate(id);
  }

  if (id == "DEQUALINIUM_025") {
    if (!s.hasSymptom(Symptom::MildSoreThroat)) return notApplicable(id);
    if (s.hasAllergy(DrugAllergy::Dequalinium)) {
      return stop(RuleDisposition::Deny, RuleReason::Allergy, id);
    }
    return candidate(id);
  }

  if (id == "SIMETHICONE_80") {
    if (!s.hasSymptom(Symptom::GasBloating)) return notApplicable(id);
    if (s.hasAllergy(DrugAllergy::Simethicone)) {
      return stop(RuleDisposition::Deny, RuleReason::Allergy, id);
    }
    if (s.hasCondition(Condition::ThyroidTreatment) ||
        s.usesMedicine(MedicineGroup::Levothyroxine)) {
      return stop(RuleDisposition::Refer, RuleReason::Interaction, id);
    }
    return candidate(id);
  }

  if (id == "ANTACID_200_200") {
    if (!s.hasSymptom(Symptom::AcidIndigestion)) return notApplicable(id);
    if (s.hasAllergy(DrugAllergy::AluminiumOrMagnesiumAntacid)) {
      return stop(RuleDisposition::Deny, RuleReason::Allergy, id);
    }
    if (s.hasCondition(Condition::KidneyDisease) ||
        s.hasCondition(Condition::LiverDisease) ||
        s.hasCondition(Condition::HeartDisease) ||
        s.hasCondition(Condition::SodiumRestriction) ||
        s.usesMedicine(MedicineGroup::MedicineRequiringAntacidSpacing)) {
      return stop(RuleDisposition::Refer, RuleReason::Interaction, id);
    }
    return candidate(id);
  }

  if (id == "OMEPRAZOLE_10") {
    if (!s.hasSymptom(Symptom::ShortTermReflux)) return notApplicable(id);
    if (s.ageYears < 18) {
      return stop(RuleDisposition::Refer, RuleReason::AgeRequiresPharmacist,
                  id);
    }
    if (s.hasAllergy(DrugAllergy::OmeprazoleOrPpi) ||
        s.usesMedicine(MedicineGroup::Clopidogrel) ||
        s.usesMedicine(MedicineGroup::ImportantPpiInteraction)) {
      return stop(RuleDisposition::Deny, RuleReason::Interaction, id);
    }
    return candidate(id);
  }

  if (id == "LOPERAMIDE_2") {
    if (!s.hasSymptom(Symptom::AcuteWateryDiarrhoea)) return notApplicable(id);
    if (!s.diarrhoeaAfterAntibioticsKnown) {
      return stop(RuleDisposition::NeedMoreInfo,
                  RuleReason::MissingConditionalAnswer, id);
    }
    if (s.hasAllergy(DrugAllergy::Loperamide) ||
        s.diarrhoeaAfterAntibiotics || s.hasSymptom(Symptom::Fever) ||
        s.hasCondition(Condition::InflammatoryBowelDisease) ||
        s.hasCondition(Condition::Constipation) ||
        s.hasCondition(Condition::AbdominalSwelling)) {
      return stop(RuleDisposition::Deny, RuleReason::Contraindication, id);
    }
    if (s.durationHours > 48) {
      return stop(RuleDisposition::Deny, RuleReason::DurationTooLong, id);
    }
    return candidate(id);
  }

  if (id == "BISACODYL_5") {
    if (!s.hasSymptom(Symptom::ShortTermConstipation)) return notApplicable(id);
    if (s.ageYears < 18) {
      return stop(RuleDisposition::Refer, RuleReason::AgeRequiresPharmacist,
                  id);
    }
    if (!s.constipationLifestyleTriedKnown) {
      return stop(RuleDisposition::NeedMoreInfo,
                  RuleReason::MissingConditionalAnswer, id);
    }
    if (!s.constipationLifestyleTried) {
      return stop(RuleDisposition::Refer,
                  RuleReason::LifestyleMeasuresNotTried, id);
    }
    if (s.hasAllergy(DrugAllergy::Bisacodyl) ||
        s.hasCondition(Condition::InflammatoryBowelDisease) ||
        s.hasCondition(Condition::BowelObstruction) ||
        s.usesMedicine(MedicineGroup::RecentAntacid)) {
      return stop(RuleDisposition::Deny, RuleReason::Contraindication, id);
    }
    return candidate(id);
  }

  if (id == "DIMENHYDRINATE_50") {
    if (!s.hasSymptom(Symptom::MotionSickness)) return notApplicable(id);
    if (!s.alcoholUseKnown || !s.drivingStatusKnown) {
      return stop(RuleDisposition::NeedMoreInfo,
                  RuleReason::MissingConditionalAnswer, id);
    }
    if (s.hasAllergy(DrugAllergy::Dimenhydrinate) || s.alcoholUse ||
        s.drivingOrOperatingMachinery ||
        s.hasCondition(Condition::Glaucoma) ||
        s.hasCondition(Condition::AsthmaOrCopd) ||
        s.hasCondition(Condition::UrinationDifficulty) ||
        s.usesMedicine(MedicineGroup::SedativeOrTranquilizer)) {
      return stop(RuleDisposition::Deny, RuleReason::Contraindication, id);
    }
    return candidate(id);
  }

  if (id == "S_BOULARDII_250") {
    if (!(s.hasSymptom(Symptom::DigestiveSupport) ||
          s.hasSymptom(Symptom::AcuteWateryDiarrhoea))) {
      return notApplicable(id);
    }
    if (s.hasAllergy(DrugAllergy::Yeast) ||
        s.hasCondition(Condition::YeastAllergy) ||
        s.hasCondition(Condition::Immunocompromised) ||
        s.hasCondition(Condition::CriticallyIll) ||
        s.hasCondition(Condition::CentralVenousCatheter)) {
      return stop(RuleDisposition::Deny, RuleReason::Contraindication, id);
    }
    return candidate(id);
  }

  return notApplicable(id);
}

}  // namespace smv::medical
