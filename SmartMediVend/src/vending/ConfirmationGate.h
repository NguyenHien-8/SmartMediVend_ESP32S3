#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace smv::vending {

enum class ConfirmationIntent : uint8_t { Unknown = 0, Affirm, Reject };

class ConfirmationGate {
 public:
  void arm(std::string_view sessionId,
           std::string_view candidateId,
           uint32_t nowMs,
           uint32_t timeoutMs);
  void invalidate();
  bool confirm(std::string_view sessionId,
               std::string_view candidateId,
               std::string_view transcript,
               uint32_t nowMs);
  bool isArmed() const { return armed_; }
  bool isExpired(uint32_t nowMs) const;

  static ConfirmationIntent classify(std::string_view transcript);

 private:
  std::string sessionId_;
  std::string candidateId_;
  uint32_t armedAtMs_ = 0;
  uint32_t timeoutMs_ = 0;
  bool armed_ = false;
};

}  // namespace smv::vending
