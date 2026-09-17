#include "TestHarness.h"

#include "src/medical/MedicineCatalog.h"
#include "src/medical/PharmacistGate.h"

using smv::medical::MedicineCatalog;
using smv::medical::PharmacistGate;
using smv::medical::ReviewArtifact;

TEST_CASE("catalog has 16 slots, 13 medicines, and exact backups") {
  const auto& catalog = MedicineCatalog::builtIn();

  REQUIRE(catalog.slotCount() == 16);
  REQUIRE(catalog.medicineCount() == 13);
  REQUIRE(catalog.slotByChannel(13)->backupOfChannel == 0);
  REQUIRE(catalog.slotByChannel(14)->backupOfChannel == 3);
  REQUIRE(catalog.slotByChannel(15)->backupOfChannel == 7);
  REQUIRE(catalog.slotByChannel(0)->initialStock == 5);
  REQUIRE(catalog.slotByChannel(15)->initialStock == 5);
}

TEST_CASE("catalog backup channels point to the same canonical medicine") {
  const auto& catalog = MedicineCatalog::builtIn();

  REQUIRE(catalog.slotByChannel(0)->canonicalId ==
          catalog.slotByChannel(13)->canonicalId);
  REQUIRE(catalog.slotByChannel(3)->canonicalId ==
          catalog.slotByChannel(14)->canonicalId);
  REQUIRE(catalog.slotByChannel(7)->canonicalId ==
          catalog.slotByChannel(15)->canonicalId);
}

TEST_CASE("pharmacist gate denies unapproved or version-mismatched review") {
  constexpr auto catalogVersion = "catalog-2026-09-17";
  constexpr auto rulesVersion = "rules-2026-09-17";

  REQUIRE_FALSE(PharmacistGate::allowsProductionVending(
      ReviewArtifact{false, catalogVersion, rulesVersion}, catalogVersion,
      rulesVersion));
  REQUIRE_FALSE(PharmacistGate::allowsProductionVending(
      ReviewArtifact{true, "wrong", rulesVersion}, catalogVersion,
      rulesVersion));
  REQUIRE_FALSE(PharmacistGate::allowsProductionVending(
      ReviewArtifact{true, catalogVersion, "wrong"}, catalogVersion,
      rulesVersion));
  REQUIRE(PharmacistGate::allowsProductionVending(
      ReviewArtifact{true, catalogVersion, rulesVersion}, catalogVersion,
      rulesVersion));
}
