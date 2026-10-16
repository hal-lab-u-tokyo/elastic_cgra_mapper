#include "engine.hpp"

namespace mapper::detail::prisa_array {

namespace {

double Percentile(std::vector<int> values, double percentile) {
  if (values.empty()) return 0.0;
  std::sort(values.begin(), values.end());
  const double rank = percentile / 100.0 * (values.size() - 1);
  const int lower = static_cast<int>(std::floor(rank));
  const int upper = static_cast<int>(std::ceil(rank));
  if (lower == upper) return values[lower];
  const double fraction = rank - lower;
  return values[lower] * (1.0 - fraction) + values[upper] * fraction;
}

void WriteCost(std::ofstream& trace, double value) {
  if (value >= kImpossibleCost * 0.5 || !std::isfinite(value)) return;
  trace << value;
}

}  // namespace

std::string Placement2DPRISAEngine::MapperName() const {
  switch (kind_) {
    case mapper::Placement2DArrayKind::kPRISA:
      return "Placement2DArrayPRISA";
    case mapper::Placement2DArrayKind::kPRISANoSIS:
      return "Placement2DArrayPRISANoSIS";
    case mapper::Placement2DArrayKind::kCostAwarePRISA:
      return "Placement2DArrayCostAwarePRISA";
  }
  return "Placement2DArray";
}

bool Placement2DPRISAEngine::UsesPRISASIS() const {
  return kind_ == mapper::Placement2DArrayKind::kPRISA ||
         kind_ == mapper::Placement2DArrayKind::kCostAwarePRISA;
}

bool Placement2DPRISAEngine::UsesCostAwarePRISA() const {
  return kind_ == mapper::Placement2DArrayKind::kCostAwarePRISA;
}

bool Placement2DPRISAEngine::UsesStructuralIOCellTypes() const {
  return io_node_policy_ == "structural" && (false || false);
}

int Placement2DPRISAEngine::MaxTrials() const {
  if (max_trials_.has_value()) return std::max(1, max_trials_.value());
  return std::max(100, dfg_.GetNodeNum() * 4);
}

int Placement2DPRISAEngine::SeedCount() const {
  if (seed_count_.has_value()) return std::max(1, seed_count_.value());
  return 1;
}

unsigned int Placement2DPRISAEngine::SeedFor(int seed_index) const {
  const unsigned int seed =
      random_seed_.has_value() ? static_cast<unsigned int>(random_seed_.value())
                               : base_seed_;
  return seed + static_cast<unsigned int>(seed_index * 0x9E3779B9u);
}

void Placement2DPRISAEngine::ResetSeed(int seed_index) {
  const unsigned int seed = SeedFor(seed_index);
  rng_.seed(seed);
  std::srand(seed);
}

bool Placement2DPRISAEngine::HasTimedOut(
    std::chrono::steady_clock::time_point start, double reserve_s) const {
  return SecondsSince(start) + reserve_s >= timeout_s_;
}

void Placement2DPRISAEngine::Log(const std::string& message) const {
  if (!log_file_path_.has_value()) return;
  std::ofstream ofs(log_file_path_.value(), std::ios::app);
  if (ofs) ofs << "[" << MapperName() << "] " << message << "\n";
}

std::string Placement2DPRISAEngine::NodeLabel(int node) const {
  if (node < 0 || node >= dfg_.GetNodeNum()) return std::to_string(node);
  const auto property = dfg_.GetNodeProperty(node);
  return std::to_string(node) + ":" + property.op_name + "/" + property.op_str;
}

}  // namespace mapper::detail::prisa_array
