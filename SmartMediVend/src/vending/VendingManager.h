#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

#include "../inventory/InventoryManager.h"
#include "RelayDriver.h"
#include "VendGuard.h"

namespace smv::vending {

struct VendRequest {
  std::string transactionId;
  std::array<std::string, 3> canonicalIds{};
  std::size_t count = 0;
};

enum class VendStartResult : uint8_t {
  Started = 0,
  InvalidRequest,
  GuardDenied,
  OutOfStock,
  Busy,
  DuplicateTransaction
};

enum class VendingState : uint8_t {
  Idle = 0,
  Running,
  Succeeded,
  Failed
};

class VendingManager {
 public:
  VendingManager(inventory::InventoryManager& inventory, RelayDriver& relay)
      : inventory_(inventory), relay_(relay) {}

  VendStartResult start(const VendRequest& request,
                        VendContext context,
                        uint32_t nowMs);
  void process(uint32_t nowMs);
  void cancel();

  bool succeeded() const { return state_ == VendingState::Succeeded; }
  bool active() const { return state_ == VendingState::Running; }
  VendingState state() const { return state_; }

 private:
  bool startCurrentPulse(uint32_t nowMs);

  inventory::InventoryManager& inventory_;
  RelayDriver& relay_;
  VendGuard guard_;
  VendRequest request_;
  std::array<uint8_t, 3> channels_{};
  std::size_t currentIndex_ = 0;
  VendingState state_ = VendingState::Idle;
};

}  // namespace smv::vending
