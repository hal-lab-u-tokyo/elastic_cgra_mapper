#include "../array_engine_internal.hpp"

namespace mapper::detail::placement2d {

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

std::string Placement2DArrayEngine::MapperName() const {
  switch (kind_) {
    case mapper::Placement2DArrayKind::kPaperGuidedYOTO:
      return "Placement2DPaperGuidedArrayYOTO";
    case mapper::Placement2DArrayKind::kPaperGuidedYOTT:
      return "Placement2DPaperGuidedArrayYOTT";
    case mapper::Placement2DArrayKind::kSA:
      return "Placement2DArraySA";
    case mapper::Placement2DArrayKind::kCPUMappingYOTO:
      return "Placement2DCPUMappingYOTO";
    case mapper::Placement2DArrayKind::kCPUMappingYOTT:
      return "Placement2DCPUMappingYOTT";
    case mapper::Placement2DArrayKind::kCPUMappingYOTTCore:
      return "Placement2DCPUMappingYOTTCore";
    case mapper::Placement2DArrayKind::kCPUMappingYOTTCoreRepair:
      return "Placement2DCPUMappingYOTTCoreRepair";
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
         kind_ == mapper::Placement2DArrayKind::kCPUMappingYOTT ||
         kind_ == mapper::Placement2DArrayKind::kCPUMappingYOTTCore ||
         kind_ == mapper::Placement2DArrayKind::kCPUMappingYOTTCoreRepair;
}

bool Placement2DArrayEngine::IsPaperGuidedTraversalLike() const {
  return kind_ == mapper::Placement2DArrayKind::kPaperGuidedYOTO ||
         kind_ == mapper::Placement2DArrayKind::kPaperGuidedYOTT;
}

bool Placement2DArrayEngine::IsPaperGuidedYOTO() const {
  return kind_ == mapper::Placement2DArrayKind::kPaperGuidedYOTO;
}

bool Placement2DArrayEngine::IsPaperGuidedYOTT() const {
  return kind_ == mapper::Placement2DArrayKind::kPaperGuidedYOTT;
}

bool Placement2DArrayEngine::IsCPUMappingYOTO() const {
  return kind_ == mapper::Placement2DArrayKind::kCPUMappingYOTO;
}

bool Placement2DArrayEngine::IsCPUMappingYOTT() const {
  return kind_ == mapper::Placement2DArrayKind::kCPUMappingYOTT ||
         kind_ == mapper::Placement2DArrayKind::kCPUMappingYOTTCore ||
         kind_ == mapper::Placement2DArrayKind::kCPUMappingYOTTCoreRepair;
}

bool Placement2DArrayEngine::IsCPUMappingYOTTCore() const {
  return kind_ == mapper::Placement2DArrayKind::kCPUMappingYOTTCore;
}

bool Placement2DArrayEngine::IsCPUMappingYOTTCoreRepair() const {
  return kind_ == mapper::Placement2DArrayKind::kCPUMappingYOTTCoreRepair;
}

bool Placement2DArrayEngine::UsesYOTTAnnotations() const {
  if (use_yott_annotations_.has_value()) {
    return use_yott_annotations_.value();
  }
  return IsPaperGuidedYOTT() || IsCPUMappingYOTT();
}

bool Placement2DArrayEngine::UsesStructuralIOCellTypes() const {
  return io_node_policy_ == "structural" &&
         (IsCPUMappingLike() || IsPaperGuidedTraversalLike());
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

int Placement2DArrayEngine::ElitePlacementCount() const {
  if (elite_placement_count_.has_value()) {
    return std::max(1, elite_placement_count_.value());
  }
  return 8;
}

unsigned int Placement2DArrayEngine::SeedFor(int seed_index) const {
  const unsigned int seed =
      random_seed_.has_value() ? static_cast<unsigned int>(random_seed_.value())
                               : base_seed_;
  return seed + static_cast<unsigned int>(seed_index * 0x9E3779B9u);
}

unsigned int Placement2DArrayEngine::TrialSeedFor(int seed_index,
                                                  int trial_index) const {
  unsigned int seed = SeedFor(seed_index);
  seed ^= static_cast<unsigned int>(trial_index + 1) * 0x85EBCA6Bu;
  seed ^= seed >> 16;
  seed *= 0x7FEB352Du;
  seed ^= seed >> 15;
  return seed;
}

void Placement2DArrayEngine::ResetSeed(int seed_index) {
  const unsigned int seed = SeedFor(seed_index);
  rng_.seed(seed);
  std::srand(seed);
}

void Placement2DArrayEngine::ResetTrialSeed(int seed_index, int trial_index) {
  const unsigned int seed = TrialSeedFor(seed_index, trial_index);
  rng_.seed(seed);
  std::srand(seed);
}

bool Placement2DArrayEngine::UsesPerTrialSeeds() const {
  return trial_seed_policy_ == "per_trial";
}

int Placement2DArrayEngine::AuthorRandomInt(int exclusive_upper_bound) {
  if (exclusive_upper_bound <= 1) return 0;
  return std::rand() % exclusive_upper_bound;
}

void Placement2DArrayEngine::ShuffleWithAuthorRandom(std::vector<int>& values) {
  for (int i = 1; i < static_cast<int>(values.size()); i++) {
    std::swap(values[i], values[AuthorRandomInt(i + 1)]);
  }
}

void Placement2DArrayEngine::ShuffleWithAuthorRandom(
    std::array<std::pair<int, int>, 4>& values) {
  for (int i = 1; i < static_cast<int>(values.size()); i++) {
    std::swap(values[i], values[AuthorRandomInt(i + 1)]);
  }
}

int Placement2DArrayEngine::TraversalNeighborMode(bool is_yott) {
  if (traversal_neighbor_policy_ == "degree") return 0;
  if (traversal_neighbor_policy_ == "betweenness") return 1;
  if (traversal_neighbor_policy_ == "critical_path") return 2;
  if (traversal_neighbor_policy_ == "zigzag") return 3;
  if (traversal_neighbor_policy_ == "random") return 4;
  if (traversal_neighbor_policy_ != "default") {
    Log("unknown traversal_neighbor_policy=" + traversal_neighbor_policy_ +
        "; using default");
  }
  if (!is_yott) return 0;
  if (IsCPUMappingYOTT()) return AuthorRandomInt(4);
  std::uniform_int_distribution<int> mode_dist(0, 3);
  return mode_dist(rng_);
}

bool Placement2DArrayEngine::HasTimedOut(
    std::chrono::steady_clock::time_point start, double reserve_s) const {
  return SecondsSince(start) + reserve_s >= timeout_s_;
}

void Placement2DArrayEngine::Log(const std::string& message) const {
  if (!log_file_path_.has_value()) return;
  std::ofstream ofs(log_file_path_.value(), std::ios::app);
  if (ofs) ofs << "[" << MapperName() << "] " << message << "\n";
}

bool Placement2DArrayEngine::TraceTrials() const {
  return trace_trials_.value_or(false) && log_file_path_.has_value();
}

std::string Placement2DArrayEngine::TrialTracePath() const {
  if (!log_file_path_.has_value()) return "trial_trace.csv";
  const std::string path = log_file_path_.value();
  const std::string::size_type slash = path.find_last_of("/\\");
  if (slash == std::string::npos) return "trial_trace.csv";
  return path.substr(0, slash + 1) + "trial_trace.csv";
}

void Placement2DArrayEngine::WriteTrialTraceHeader(std::ofstream& trace) const {
  trace << "mapper,phase,seed_index,trial_index,success,selected_as_best,"
           "selection_primary,selection_secondary,placement_cost,"
           "mesh_hop_sum,avg_mesh_hop,max_mesh_hop,"
           "mesh_optimal_edge_ratio,paper_optimal_edge_ratio,"
           "avg_mesh_fifo,max_mesh_fifo,p95_mesh_fifo,"
           "avg_paper_fifo,max_paper_fifo,elapsed_sec,trial_seed,"
           "plan_hash,placement_hash,placement_signature\n";
}

TrialTraceMetrics Placement2DArrayEngine::ComputeTrialTraceMetrics(
    const PlacementState& state) const {
  TrialTraceMetrics metrics;
  metrics.placement_cost = 0.0;

  std::vector<int> mesh_fifo_values;
  mesh_fifo_values.reserve(edges_.size());
  int mesh_optimal_edges = 0;
  int paper_optimal_edges = 0;
  int paper_fifo_sum = 0;

  for (const auto& edge : edges_) {
    const int source_cell = state.dfg_to_cell[edge.source];
    const int target_cell = state.dfg_to_cell[edge.target];
    if (source_cell < 0 || target_cell < 0) {
      metrics.placement_cost = kImpossibleCost;
      return metrics;
    }

    const int dx = std::abs(Row(source_cell) - Row(target_cell));
    const int dy = std::abs(Col(source_cell) - Col(target_cell));
    const int mesh_hop = dx + dy;
    const int placement_cost = DistanceCost(source_cell, target_cell);
    const int mesh_fifo = std::max(0, mesh_hop - 1);
    const int paper_fifo = std::max(0, placement_cost - 1);

    metrics.placement_cost += placement_cost;
    metrics.mesh_hop_sum += mesh_hop;
    metrics.max_mesh_hop = std::max(metrics.max_mesh_hop, mesh_hop);
    metrics.max_mesh_fifo = std::max(metrics.max_mesh_fifo, mesh_fifo);
    metrics.max_paper_fifo = std::max(metrics.max_paper_fifo, paper_fifo);
    mesh_fifo_values.push_back(mesh_fifo);
    paper_fifo_sum += paper_fifo;
    if (mesh_hop <= 1) mesh_optimal_edges++;
    if (placement_cost <= 1) paper_optimal_edges++;
  }

  const int edge_count = static_cast<int>(edges_.size());
  if (edge_count > 0) {
    metrics.avg_mesh_hop = metrics.mesh_hop_sum / edge_count;
    metrics.mesh_optimal_edge_ratio =
        static_cast<double>(mesh_optimal_edges) / edge_count;
    metrics.paper_optimal_edge_ratio =
        static_cast<double>(paper_optimal_edges) / edge_count;
    metrics.avg_mesh_fifo =
        std::accumulate(mesh_fifo_values.begin(), mesh_fifo_values.end(), 0.0) /
        edge_count;
    metrics.p95_mesh_fifo = Percentile(mesh_fifo_values, 95.0);
    metrics.avg_paper_fifo = static_cast<double>(paper_fifo_sum) / edge_count;
  }
  return metrics;
}

std::string Placement2DArrayEngine::PlacementSignature(
    const PlacementState& state) const {
  std::ostringstream out;
  for (int node = 0; node < static_cast<int>(state.dfg_to_cell.size());
       node++) {
    if (node > 0) out << ':';
    out << state.dfg_to_cell[node];
  }
  return out.str();
}

std::string Placement2DArrayEngine::PlacementHash(
    const PlacementState& state) const {
  std::uint64_t hash = 1469598103934665603ULL;
  for (int cell : state.dfg_to_cell) {
    hash ^= static_cast<std::uint64_t>(static_cast<std::uint32_t>(cell + 1));
    hash *= 1099511628211ULL;
  }
  std::ostringstream out;
  out << std::hex << hash;
  return out.str();
}

std::string Placement2DArrayEngine::PlanHash(
    const std::vector<Step>& plan) const {
  std::uint64_t hash = 1469598103934665603ULL;
  const auto append = [&hash](int value) {
    hash ^= static_cast<std::uint64_t>(static_cast<std::uint32_t>(value + 1));
    hash *= 1099511628211ULL;
  };
  for (const auto& step : plan) {
    append(step.anchor);
    append(step.target);
    append(step.edge_id);
    append(static_cast<int>(step.annotations.size()));
    for (const auto& annotation : step.annotations) {
      append(static_cast<int>(annotation.kind));
      append(annotation.anchor_node);
      append(annotation.distance);
    }
  }
  std::ostringstream out;
  out << std::hex << hash;
  return out.str();
}

void Placement2DArrayEngine::WriteTrialTraceRow(
    std::ofstream& trace, const std::string& phase, int seed_index,
    int trial_index, bool success, bool selected_as_best,
    double selection_primary, double selection_secondary,
    const std::optional<PlacementState>& state, double elapsed_s,
    const std::string& plan_hash) const {
  const TrialTraceMetrics metrics = state.has_value()
                                        ? ComputeTrialTraceMetrics(*state)
                                        : TrialTraceMetrics{};

  trace << MapperName() << "," << phase << "," << seed_index << ","
        << trial_index << "," << (success ? 1 : 0) << ","
        << (selected_as_best ? 1 : 0) << ",";
  WriteCost(trace, selection_primary);
  trace << ",";
  WriteCost(trace, selection_secondary);
  trace << ",";
  WriteCost(trace, metrics.placement_cost);
  trace << "," << metrics.mesh_hop_sum << "," << metrics.avg_mesh_hop << ","
        << metrics.max_mesh_hop << "," << metrics.mesh_optimal_edge_ratio << ","
        << metrics.paper_optimal_edge_ratio << "," << metrics.avg_mesh_fifo
        << "," << metrics.max_mesh_fifo << "," << metrics.p95_mesh_fifo << ","
        << metrics.avg_paper_fifo << "," << metrics.max_paper_fifo << ","
        << elapsed_s << ",";
  if (UsesPerTrialSeeds() && seed_index >= 0 && trial_index >= 0) {
    trace << TrialSeedFor(seed_index, trial_index);
  }
  trace << "," << plan_hash << ",";
  if (state.has_value()) {
    trace << PlacementHash(*state) << "," << PlacementSignature(*state);
  } else {
    trace << ",";
  }
  trace << "\n";
}

void Placement2DArrayEngine::RecordPlacementSwapAttempts(
    long long count) const {
  if (count > 0) placement_swap_attempts_ += count;
}

std::string Placement2DArrayEngine::NodeLabel(int node) const {
  if (node < 0 || node >= dfg_.GetNodeNum()) return std::to_string(node);
  const auto property = dfg_.GetNodeProperty(node);
  return std::to_string(node) + ":" + property.op_name + "/" + property.op_str;
}

int Placement2DArrayEngine::PlacedCount(const PlacementState& state) const {
  int count = 0;
  for (int cell : state.dfg_to_cell) {
    if (cell >= 0) count++;
  }
  return count;
}

int Placement2DArrayEngine::FreeCPUMappingCompatibleCellCount(
    int dfg_node, const PlacementState& state) const {
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
