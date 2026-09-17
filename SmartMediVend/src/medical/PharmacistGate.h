#pragma once

#include <string_view>

namespace smv::medical {

struct ReviewArtifact {
  bool approved;
  std::string_view reviewedCatalogVersion;
  std::string_view reviewedRulesVersion;
};

class PharmacistGate {
 public:
  static bool allowsProductionVending(
      const ReviewArtifact& review,
      std::string_view activeCatalogVersion,
      std::string_view activeRulesVersion);
};

}  // namespace smv::medical
