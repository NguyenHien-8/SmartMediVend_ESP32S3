#include "PharmacistGate.h"

namespace smv::medical {

bool PharmacistGate::allowsProductionVending(
    const ReviewArtifact& review,
    std::string_view activeCatalogVersion,
    std::string_view activeRulesVersion) {
  return review.approved &&
         review.reviewedCatalogVersion == activeCatalogVersion &&
         review.reviewedRulesVersion == activeRulesVersion;
}

}  // namespace smv::medical
