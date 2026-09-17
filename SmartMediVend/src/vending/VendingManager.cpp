#include "VendingManager.h"

namespace smv::vending {

VendStartResult VendingManager::start(const VendRequest& request,
                                      VendContext context,
                                      uint32_t nowMs) {
  if (active()) return VendStartResult::Busy;
  if (request.transactionId.empty() || request.count == 0 ||
      request.count > request.canonicalIds.size()) {
    return VendStartResult::InvalidRequest;
  }
  if (inventory_.hasAnyCommit(request.transactionId)) {
    return VendStartResult::DuplicateTransaction;
  }

  for (std::size_t index = 0; index < request.count; ++index) {
    if (request.canonicalIds[index].empty()) {
      return VendStartResult::InvalidRequest;
    }
    const int channel = inventory_.resolveChannel(request.canonicalIds[index]);
    if (channel < 0) return VendStartResult::OutOfStock;
    channels_[index] = static_cast<uint8_t>(channel);
    for (std::size_t prior = 0; prior < index; ++prior) {
      if (channels_[prior] == channels_[index]) {
        return VendStartResult::InvalidRequest;
      }
    }
  }

  context.duplicateRequest = false;
  context.transactionActive = false;
  context.inventoryAvailable = true;
  context.relayHealthy = relay_.isHealthy();
  if (!guard_.authorize(context).allowed()) {
    return VendStartResult::GuardDenied;
  }

  request_ = request;
  currentIndex_ = 0;
  state_ = VendingState::Running;
  if (!startCurrentPulse(nowMs)) {
    state_ = VendingState::Failed;
    return VendStartResult::Busy;
  }
  return VendStartResult::Started;
}

void VendingManager::process(uint32_t nowMs) {
  if (!active()) return;

  relay_.process(nowMs);
  const int completedChannel = relay_.takeCompletedChannel();
  if (completedChannel >= 0) {
    const auto committed = inventory_.commitPulse(
        request_.transactionId, static_cast<uint8_t>(completedChannel));
    if (committed != inventory::CommitResult::Committed &&
        committed != inventory::CommitResult::AlreadyCommitted) {
      relay_.allOff();
      state_ = VendingState::Failed;
      return;
    }
    ++currentIndex_;
  }

  if (!relay_.isIdle()) return;
  if (currentIndex_ >= request_.count) {
    state_ = VendingState::Succeeded;
    return;
  }
  if (!startCurrentPulse(nowMs)) {
    relay_.allOff();
    state_ = VendingState::Failed;
  }
}

void VendingManager::cancel() {
  relay_.allOff();
  state_ = VendingState::Failed;
}

bool VendingManager::startCurrentPulse(uint32_t nowMs) {
  return currentIndex_ < request_.count &&
         relay_.startPulse(channels_[currentIndex_], nowMs);
}

}  // namespace smv::vending
