#include "ConfirmationGate.h"

#include <algorithm>
#include <array>
#include <cctype>

#include "../core/Elapsed.h"

namespace smv::vending {
namespace {

std::string trimAndLowerAscii(std::string_view input) {
  std::size_t begin = 0;
  while (begin < input.size() &&
         std::isspace(static_cast<unsigned char>(input[begin])) != 0) {
    ++begin;
  }
  std::size_t end = input.size();
  while (end > begin &&
         std::isspace(static_cast<unsigned char>(input[end - 1])) != 0) {
    --end;
  }
  std::string output(input.substr(begin, end - begin));
  std::transform(output.begin(), output.end(), output.begin(), [](char value) {
    const auto byte = static_cast<unsigned char>(value);
    return byte < 0x80U ? static_cast<char>(std::tolower(byte)) : value;
  });
  return output;
}

}  // namespace

void ConfirmationGate::arm(std::string_view sessionId,
                           std::string_view candidateId,
                           uint32_t nowMs,
                           uint32_t timeoutMs) {
  sessionId_ = sessionId;
  candidateId_ = candidateId;
  armedAtMs_ = nowMs;
  timeoutMs_ = timeoutMs;
  armed_ = !sessionId_.empty() && !candidateId_.empty() && timeoutMs_ > 0;
}

void ConfirmationGate::invalidate() {
  armed_ = false;
  sessionId_.clear();
  candidateId_.clear();
}

bool ConfirmationGate::confirm(std::string_view sessionId,
                               std::string_view candidateId,
                               std::string_view transcript,
                               uint32_t nowMs) {
  if (!armed_ || isExpired(nowMs) || sessionId != sessionId_ ||
      candidateId != candidateId_ ||
      classify(transcript) != ConfirmationIntent::Affirm) {
    return false;
  }
  invalidate();
  return true;
}

bool ConfirmationGate::isExpired(uint32_t nowMs) const {
  return !armed_ || elapsedMs(nowMs, armedAtMs_, timeoutMs_);
}

ConfirmationIntent ConfirmationGate::classify(std::string_view transcript) {
  const std::string normalized = trimAndLowerAscii(transcript);
  constexpr std::array<std::string_view, 8> affirmative = {
      "đồng ý", "ĐỒNG Ý", "xac nhan", "xác nhận",
      "XÁC NHẬN", "lay thuoc", "lấy thuốc", "có"};
  constexpr std::array<std::string_view, 6> negative = {
      "không", "khong", "hủy", "huy", "từ chối", "tu choi"};

  for (const auto phrase : affirmative) {
    if (normalized == trimAndLowerAscii(phrase)) {
      return ConfirmationIntent::Affirm;
    }
  }
  for (const auto phrase : negative) {
    if (normalized == trimAndLowerAscii(phrase)) {
      return ConfirmationIntent::Reject;
    }
  }
  return ConfirmationIntent::Unknown;
}

}  // namespace smv::vending
