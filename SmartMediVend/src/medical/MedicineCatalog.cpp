#include "MedicineCatalog.h"

namespace smv::medical {

MedicineCatalog::MedicineCatalog()
    : medicines_{{
          {"PARACETAMOL_500", "Paracetamol 500", "Paracetamol", "500 mg",
           "fever_mild_pain"},
          {"IBUPROFEN_200", "Ibuprofen 200", "Ibuprofen", "200 mg",
           "mild_inflammatory_pain"},
          {"LORATADINE_10", "Loratadine 10", "Loratadine", "10 mg",
           "allergic_rhinitis"},
          {"DEXTROMETHORPHAN_15", "Dextromethorphan 15",
           "Dextromethorphan HBr", "15 mg", "dry_cough"},
          {"AMBROXOL_30", "Ambroxol 30", "Ambroxol HCl", "30 mg",
           "productive_cough"},
          {"DEQUALINIUM_025", "Dequalinium 0.25", "Dequalinium chloride",
           "0.25 mg lozenge", "mild_sore_throat"},
          {"SIMETHICONE_80", "Simethicone 80", "Simethicone", "80 mg",
           "gas_bloating"},
          {"ANTACID_200_200", "Antacid chewable",
           "Aluminium hydroxide + Magnesium hydroxide", "200 mg + 200 mg",
           "acid_indigestion"},
          {"OMEPRAZOLE_10", "Omeprazole 10", "Omeprazole", "10 mg",
           "short_term_reflux"},
          {"LOPERAMIDE_2", "Loperamide 2", "Loperamide HCl", "2 mg",
           "acute_watery_diarrhoea"},
          {"BISACODYL_5", "Bisacodyl 5", "Bisacodyl", "5 mg",
           "short_term_constipation"},
          {"DIMENHYDRINATE_50", "Dimenhydrinate 50", "Dimenhydrinate",
           "50 mg", "motion_sickness"},
          {"S_BOULARDII_250", "Saccharomyces boulardii 250",
           "Saccharomyces boulardii", "250 mg", "digestive_support"},
      }},
      slots_{{
          {0, "SMV-PARA500", "PARACETAMOL_500", -1, 5},
          {1, "SMV-IBU200", "IBUPROFEN_200", -1, 5},
          {2, "SMV-LOR10", "LORATADINE_10", -1, 5},
          {3, "SMV-DXM15", "DEXTROMETHORPHAN_15", -1, 5},
          {4, "SMV-AMB30", "AMBROXOL_30", -1, 5},
          {5, "SMV-DEQ025", "DEQUALINIUM_025", -1, 5},
          {6, "SMV-SIM80", "SIMETHICONE_80", -1, 5},
          {7, "SMV-ANTACID", "ANTACID_200_200", -1, 5},
          {8, "SMV-OME10", "OMEPRAZOLE_10", -1, 5},
          {9, "SMV-LOP2", "LOPERAMIDE_2", -1, 5},
          {10, "SMV-BIS5", "BISACODYL_5", -1, 5},
          {11, "SMV-DIM50", "DIMENHYDRINATE_50", -1, 5},
          {12, "SMV-SB250", "S_BOULARDII_250", -1, 5},
          {13, "SMV-PARA500-B", "PARACETAMOL_500", 0, 5},
          {14, "SMV-DXM15-B", "DEXTROMETHORPHAN_15", 3, 5},
          {15, "SMV-ANTACID-B", "ANTACID_200_200", 7, 5},
      }} {}

const MedicineCatalog& MedicineCatalog::builtIn() {
  static const MedicineCatalog catalog;
  return catalog;
}

const PhysicalSlot* MedicineCatalog::slotByChannel(uint8_t channel) const {
  if (channel >= slots_.size()) {
    return nullptr;
  }
  return &slots_[channel];
}

const Medicine* MedicineCatalog::findMedicine(
    std::string_view canonicalId) const {
  for (const auto& medicine : medicines_) {
    if (medicine.canonicalId == canonicalId) {
      return &medicine;
    }
  }
  return nullptr;
}

}  // namespace smv::medical
