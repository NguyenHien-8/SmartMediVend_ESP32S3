#pragma once

#include <cstdint>

#include "../app/AppState.h"

namespace smv::vending {

enum class VendDenial : uint8_t {
  None = 0,
  InvalidState,
  InvalidCandidate,
  SafetyNotAllowed,
  MissingConfirmation,
  ConfirmationExpired,
  PharmacistApprovalRequired,
  ProductionDisabled,
  OutOfStock,
  RelayUnhealthy,
  TransactionActive,
  DuplicateTransaction,
  SessionMismatch
};

struct VendAuthorization {
  VendDenial denial = VendDenial::None;
  bool allowed() const { return denial == VendDenial::None; }
};

struct VendContext {
  AppState appState = AppState::Booting;
  bool candidateValid = false;
  bool safetyAllowed = false;
  bool userConfirmed = false;
  bool confirmationExpired = true;
  bool pharmacistApproved = false;
  bool productionVendingEnabled = false;
  bool inventoryAvailable = false;
  bool relayHealthy = false;
  bool transactionActive = false;
  bool duplicateRequest = false;
  bool sessionMatches = false;
};

class VendGuard {
 public:
  VendAuthorization authorize(const VendContext& context) const;
};

}  // namespace smv::vending
