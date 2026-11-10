#pragma once

#include <stdexcept>
#include <string>

namespace mapper {

// Optimal/suboptimal are solver-reported classifications, subject to
// tolerances. Unknown means an accepted mapping without a global optimality
// certificate.
enum class MappingStatus {
  kOptimal,
  kSuboptimal,
  kOptimalityUnknown,
  kFailure
};

constexpr bool HasMapping(MappingStatus status) {
  return status == MappingStatus::kOptimal ||
         status == MappingStatus::kSuboptimal ||
         status == MappingStatus::kOptimalityUnknown;
}

inline std::string MappingStatusToString(MappingStatus status) {
  switch (status) {
    case MappingStatus::kOptimal:
      return "optimal";
    case MappingStatus::kSuboptimal:
      return "suboptimal";
    case MappingStatus::kOptimalityUnknown:
      return "optimality_unknown";
    case MappingStatus::kFailure:
      return "failure";
  }
  throw std::invalid_argument("Unknown mapping status");
}

inline MappingStatus MappingStatusFromString(const std::string& value) {
  if (value == "optimal") return MappingStatus::kOptimal;
  if (value == "suboptimal") return MappingStatus::kSuboptimal;
  if (value == "optimality_unknown") return MappingStatus::kOptimalityUnknown;
  if (value == "failure") return MappingStatus::kFailure;
  throw std::invalid_argument("Unknown mapping status: " + value);
}

}  // namespace mapper
