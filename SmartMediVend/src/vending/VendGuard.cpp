#include "VendGuard.h"

namespace smv::vending {

VendAuthorization VendGuard::authorize(const VendContext& c) const {
  if (c.appState != AppState::AwaitingConfirmation) {
    return {VendDenial::InvalidState};
  }
  if (!c.candidateValid) return {VendDenial::InvalidCandidate};
  if (!c.safetyAllowed) return {VendDenial::SafetyNotAllowed};
  if (!c.userConfirmed) return {VendDenial::MissingConfirmation};
  if (c.confirmationExpired) return {VendDenial::ConfirmationExpired};
  if (!c.pharmacistApproved) {
    return {VendDenial::PharmacistApprovalRequired};
  }
  if (!c.productionVendingEnabled) return {VendDenial::ProductionDisabled};
  if (!c.inventoryAvailable) return {VendDenial::OutOfStock};
  if (!c.relayHealthy) return {VendDenial::RelayUnhealthy};
  if (c.transactionActive) return {VendDenial::TransactionActive};
  if (c.duplicateRequest) return {VendDenial::DuplicateTransaction};
  if (!c.sessionMatches) return {VendDenial::SessionMismatch};
  return {};
}

}  // namespace smv::vending
