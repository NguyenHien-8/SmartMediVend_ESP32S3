#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace smv::medical {

inline constexpr std::string_view kCatalogVersion = "catalog-2026-09-17";
inline constexpr std::string_view kRulesVersion = "rules-2026-09-17";

struct Medicine {
  std::string_view canonicalId;
  std::string_view displayName;
  std::string_view activeIngredient;
  std::string_view strength;
  std::string_view symptomDomain;
};

struct PhysicalSlot {
  uint8_t channel;
  std::string_view sku;
  std::string_view canonicalId;
  int8_t backupOfChannel;
  uint8_t initialStock;
};

class MedicineCatalog {
 public:
  static const MedicineCatalog& builtIn();

  constexpr std::size_t slotCount() const { return slots_.size(); }
  constexpr std::size_t medicineCount() const { return medicines_.size(); }

  const PhysicalSlot* slotByChannel(uint8_t channel) const;
  const Medicine* findMedicine(std::string_view canonicalId) const;

 private:
  MedicineCatalog();

  std::array<Medicine, 13> medicines_;
  std::array<PhysicalSlot, 16> slots_;
};

}  // namespace smv::medical
