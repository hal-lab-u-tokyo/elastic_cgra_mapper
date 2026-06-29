#include "placement2d_array_engine_internal.hpp"

namespace mapper::detail::placement2d {

std::string Placement2DArrayEngine::MapperName() const {
  switch (kind_) {
    case mapper::Placement2DArrayKind::kFaithfulYOTO:
      return "Placement2DFaithfulArrayYOTO";
    case mapper::Placement2DArrayKind::kFaithfulYOTT:
      return "Placement2DFaithfulArrayYOTT";
    case mapper::Placement2DArrayKind::kSA:
      return "Placement2DArraySA";
    case mapper::Placement2DArrayKind::kCPUMappingYOTO:
      return "Placement2DCPUMappingYOTO";
    case mapper::Placement2DArrayKind::kCPUMappingYOTT:
      return "Placement2DCPUMappingYOTT";
    case mapper::Placement2DArrayKind::kPRISA:
      return "Placement2DArrayPRISA";
    case mapper::Placement2DArrayKind::kPRISANoSIS:
      return "Placement2DArrayPRISANoSIS";
    case mapper::Placement2DArrayKind::kCostAwarePRISA:
      return "Placement2DArrayCostAwarePRISA";
  }
  return "Placement2DArray";
}

bool Placement2DArrayEngine::IsPRISALike() const {
  return kind_ == mapper::Placement2DArrayKind::kPRISA ||
         kind_ == mapper::Placement2DArrayKind::kPRISANoSIS ||
         kind_ == mapper::Placement2DArrayKind::kCostAwarePRISA;
}

bool Placement2DArrayEngine::UsesPRISASIS() const {
  return kind_ == mapper::Placement2DArrayKind::kPRISA ||
         kind_ == mapper::Placement2DArrayKind::kCostAwarePRISA;
}

bool Placement2DArrayEngine::UsesCostAwarePRISA() const {
  return kind_ == mapper::Placement2DArrayKind::kCostAwarePRISA;
}

bool Placement2DArrayEngine::IsCPUMappingLike() const {
  return kind_ == mapper::Placement2DArrayKind::kCPUMappingYOTO ||
         kind_ == mapper::Placement2DArrayKind::kCPUMappingYOTT;
}

bool Placement2DArrayEngine::IsFaithfulArrayTraversalLike() const {
  return kind_ == mapper::Placement2DArrayKind::kFaithfulYOTO ||
         kind_ == mapper::Placement2DArrayKind::kFaithfulYOTT;
}

bool Placement2DArrayEngine::IsFaithfulArrayYOTO() const {
  return kind_ == mapper::Placement2DArrayKind::kFaithfulYOTO;
}

bool Placement2DArrayEngine::IsFaithfulArrayYOTT() const {
  return kind_ == mapper::Placement2DArrayKind::kFaithfulYOTT;
}

bool Placement2DArrayEngine::IsCPUMappingYOTO() const {
  return kind_ == mapper::Placement2DArrayKind::kCPUMappingYOTO;
}

bool Placement2DArrayEngine::IsCPUMappingYOTT() const {
  return kind_ == mapper::Placement2DArrayKind::kCPUMappingYOTT;
}

bool Placement2DArrayEngine::UsesStructuralIOCellTypes() const {
  return io_node_policy_ == "structural" &&
         (IsCPUMappingLike() || IsFaithfulArrayTraversalLike());
}

int Placement2DArrayEngine::MaxTrials() const {
  if (max_trials_.has_value()) return std::max(1, max_trials_.value());
  return std::max(100, dfg_.GetNodeNum() * 4);
}

int Placement2DArrayEngine::SeedCount() const {
  if (seed_count_.has_value()) return std::max(1, seed_count_.value());
  return 1;
}

int Placement2DArrayEngine::MaxIterations() const {
  if (max_iterations_.has_value()) {
    return std::max(1, max_iterations_.value());
  }
  return std::max(5000, dfg_.GetNodeNum() * rows_ * cols_ * 20);
}

unsigned int Placement2DArrayEngine::SeedFor(int seed_index) const {
  const unsigned int seed = random_seed_.has_value()
                                ? static_cast<unsigned int>(random_seed_.value())
                                : base_seed_;
  return seed + static_cast<unsigned int>(seed_index * 0x9E3779B9u);
}

void Placement2DArrayEngine::ResetSeed(int seed_index) { rng_.seed(SeedFor(seed_index)); }

bool Placement2DArrayEngine::HasTimedOut(std::chrono::steady_clock::time_point start,
                 double reserve_s) const {
  return SecondsSince(start) + reserve_s >= timeout_s_;
}

void Placement2DArrayEngine::Log(const std::string& message) const {
  if (!log_file_path_.has_value()) return;
  std::ofstream ofs(log_file_path_.value(), std::ios::app);
  if (ofs) ofs << "[" << MapperName() << "] " << message << "\n";
}

void Placement2DArrayEngine::RecordPlacementSwapAttempts(
    long long count) const {
  if (count > 0) placement_swap_attempts_ += count;
}

std::string Placement2DArrayEngine::NodeLabel(int node) const {
  if (node < 0 || node >= dfg_.GetNodeNum()) return std::to_string(node);
  const auto property = dfg_.GetNodeProperty(node);
  return std::to_string(node) + ":" + property.op_name + "/" +
         property.op_str;
}

int Placement2DArrayEngine::PlacedCount(const PlacementState& state) const {
  int count = 0;
  for (int cell : state.dfg_to_cell) {
    if (cell >= 0) count++;
  }
  return count;
}

int Placement2DArrayEngine::FreeCPUMappingCompatibleCellCount(int dfg_node,
                                      const PlacementState& state) const {
  int count = 0;
  for (int cell : compatible_cells_[dfg_node]) {
    if (CanPlaceCPUMapping(dfg_node, cell, state)) count++;
  }
  return count;
}

void Placement2DArrayEngine::RecordFailure(const std::string& reason) {
  if (last_failure_reason_.empty()) last_failure_reason_ = reason;
}

}  // namespace mapper::detail::placement2d
