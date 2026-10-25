#include <mapper/yott_core_repair/engine/yott_core_repair_engine.hpp>

namespace mapper::yott_core_repair {
double SecondsSince(std::chrono::steady_clock::time_point start) {
  const auto now = std::chrono::steady_clock::now();
  return std::chrono::duration_cast<std::chrono::duration<double>>(now - start)
      .count();
}

YottCoreRepairEngine::YottCoreRepairEngine(
    entity::DFG& dfg, entity::MRRG& mrrg,
    mapper::yott_core_repair::Placement2DArrayKind kind,
    const mapper::yott_core_repair::Placement2DArrayOptions& options)
    : dfg_(dfg),
      mrrg_(mrrg),
      kind_(kind),
      timeout_s_(options.timeout_s > 0.0 ? options.timeout_s : 1.0),
      log_file_path_(options.log_file_path),
      max_trials_(options.max_trials),
      seed_count_(options.seed_count),
      random_seed_(options.random_seed),
      max_iterations_(options.max_iterations),
      elite_placement_count_(options.elite_placement_count),
      io_node_policy_(
          NormalizePolicy(options.io_node_policy.value_or("opcode"))),
      trial_seed_policy_(
          NormalizePolicy(options.trial_seed_policy.value_or("continuous"))),
      traversal_order_policy_(
          NormalizePolicy(options.traversal_order_policy.value_or("zigzag"))),
      traversal_neighbor_policy_(NormalizePolicy(
          options.traversal_neighbor_policy.value_or("default"))),
      candidate_scope_policy_(
          NormalizePolicy(options.candidate_scope_policy.value_or("default"))),
      candidate_rank_policy_(
          NormalizePolicy(options.candidate_rank_policy.value_or("default"))),
      use_yott_annotations_(options.use_yott_annotations),
      trace_trials_(options.trace_trials),
      config_(mrrg.GetMRRGConfig()),
      base_seed_(static_cast<unsigned int>(
          0x85EBCA6Bu ^
          static_cast<unsigned int>(dfg.GetNodeNum() * 131 + config_.row * 17 +
                                    config_.column))),
      rng_(base_seed_) {
  {
    if (!options.candidate_scope_policy.has_value()) {
      candidate_scope_policy_ = "default";
    }
    if (!options.candidate_rank_policy.has_value()) {
      candidate_rank_policy_ = "profile_repair";
    }
    if (!options.max_iterations.has_value()) {
      max_iterations_ = 16;
    }
    if (!options.use_yott_annotations.has_value()) {
      use_yott_annotations_ = true;
    }
  }
  BuildDFGCache();
  critical_path_score_ = BuildCriticalPathScore();
  critical_path_max_ = critical_path_score_.empty()
                           ? 0
                           : *std::max_element(critical_path_score_.begin(),
                                               critical_path_score_.end());
  BuildGridCache();
  BuildCompatibilityCache();
}

auto YottCoreRepairEngine::StartSearch()
    -> std::chrono::steady_clock::time_point {
  const auto start = std::chrono::steady_clock::now();
  placement_swap_attempts_ = 0;
  cell_visits_ = 0;
  Log("start mapper=" + MapperName() +
      " nodes=" + std::to_string(dfg_.GetNodeNum()) +
      " cells=" + std::to_string(rows_ * cols_) +
      " max_trials=" + std::to_string(MaxTrials()) +
      " seed_count=" + std::to_string(SeedCount()));

  return start;
}

std::optional<PlacementState> YottCoreRepairEngine::SearchPlacement(
    std::chrono::steady_clock::time_point start) {
  return RunCoreRepairMultiStart(start);
}

mapper::MappingResult YottCoreRepairEngine::Failure(
    std::chrono::steady_clock::time_point start) const {
  Log("failure mapper=" + MapperName() + " reason=" +
      (last_failure_reason_.empty() ? "no placement candidate"
                                    : last_failure_reason_));
  return mapper::MappingResult(false, entity::Mapping(config_),
                               SecondsSince(start));
}

mapper::MappingResult YottCoreRepairEngine::FinishPlacement(
    const PlacementState& placement,
    std::chrono::steady_clock::time_point start) {
  std::vector<int> dfg_to_mrrg(dfg_.GetNodeNum(), -1);
  for (int node = 0; node < dfg_.GetNodeNum(); node++) {
    const int cell = placement.dfg_to_cell[node];
    if (cell < 0 || cell >= static_cast<int>(cell_to_mrrg_.size())) {
      return mapper::MappingResult(false, entity::Mapping(config_),
                                   SecondsSince(start));
    }
    if (UsesStructuralIOCellTypes() && !IsCPUMappingCompatible(node, cell)) {
      Log("failure mapper=" + MapperName() +
          " reason=final placement violates structural I/O node=" +
          NodeLabel(node) + " cell=" + std::to_string(cell));
      return mapper::MappingResult(false, entity::Mapping(config_),
                                   SecondsSince(start));
    }
    dfg_to_mrrg[node] = cell_to_mrrg_[cell];
  }

  std::vector<std::vector<int>> empty_routes(dfg_.GetNodeNum());
  entity::Mapping mapping(mrrg_, dfg_, dfg_to_mrrg, empty_routes);
  const double elapsed = SecondsSince(start);
  Log("success mapper=" + MapperName() + " time_s=" + std::to_string(elapsed) +
      " placement_cost=" + std::to_string(PlacementCost(placement)) +
      " placement_swap_attempts=" + std::to_string(placement_swap_attempts_) +
      " cell_visits=" + std::to_string(cell_visits_));
  return mapper::MappingResult(true, mapping, elapsed);
}

void YottCoreRepairEngine::BuildDFGCache() {
  const int node_num = dfg_.GetNodeNum();
  successors_.assign(node_num, {});
  predecessors_.assign(node_num, {});
  incident_edge_ids_.assign(node_num, {});
  degree_.assign(node_num, 0);
  edges_.clear();
  for (int edge_id = 0; edge_id < dfg_.GetEdgeNum(); edge_id++) {
    const auto [source, target] = dfg_.GetEdgeSourceTarget(edge_id);
    const int local_edge_id = static_cast<int>(edges_.size());
    edges_.push_back({source, target});
    if (source >= 0 && source < node_num) {
      successors_[source].push_back(target);
      incident_edge_ids_[source].push_back(local_edge_id);
    }
    if (target >= 0 && target < node_num && target != source) {
      predecessors_[target].push_back(source);
      incident_edge_ids_[target].push_back(local_edge_id);
    }
  }
  for (int node = 0; node < node_num; node++) {
    degree_[node] =
        static_cast<int>(successors_[node].size() + predecessors_[node].size());
  }
}

void YottCoreRepairEngine::BuildGridCache() {
  rows_ = config_.row;
  cols_ = config_.column;
  cell_to_mrrg_.assign(rows_ * cols_, -1);
  cell_ops_.assign(rows_ * cols_, {});
  cell_memory_accessible_.assign(rows_ * cols_, false);
  for (int node_id = 0; node_id < mrrg_.GetNodeNum(); node_id++) {
    const auto property = mrrg_.GetNodeProperty(node_id);
    if (property.context_id != 0) continue;
    const int row = property.position_id.first;
    const int col = property.position_id.second;
    if (row < 0 || row >= rows_ || col < 0 || col >= cols_) continue;
    const int cell = Cell(row, col);
    cell_to_mrrg_[cell] = node_id;
    cell_ops_[cell] = property.supported_operations;
    cell_memory_accessible_[cell] = property.is_memory_accessible;
  }
  bool has_io_cell = false;
  bool has_compute_cell = false;
  for (int cell = 0; cell < rows_ * cols_; cell++) {
    if (cell_to_mrrg_[cell] < 0) continue;
    has_io_cell = has_io_cell || cell_memory_accessible_[cell];
    has_compute_cell = has_compute_cell || !cell_memory_accessible_[cell];
  }
  separate_io_cells_ = has_io_cell && has_compute_cell;

  // Cell connectivity depends only on the architecture. Cache it once rather
  // than rebuilding the same all-pairs table for every placement trial.
  initial_freedom_.assign(rows_ * cols_, 0);
  for (int cell = 0; cell < rows_ * cols_; cell++) {
    if (cell_to_mrrg_[cell] >= 0) initial_freedom_[cell] = CellFreedom(cell);
  }
}

void YottCoreRepairEngine::BuildCompatibilityCache() {
  compatible_cells_.assign(dfg_.GetNodeNum(), {});
  for (int node = 0; node < dfg_.GetNodeNum(); node++) {
    auto& cells = compatible_cells_[node];
    cells.reserve(cell_to_mrrg_.size());
    for (int cell = 0; cell < static_cast<int>(cell_to_mrrg_.size()); cell++) {
      if (IsCompatible(node, cell)) cells.push_back(cell);
    }
  }
}

std::string YottCoreRepairEngine::NormalizePolicy(std::string policy) {
  std::transform(
      policy.begin(), policy.end(), policy.begin(),
      [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return policy;
}

int YottCoreRepairEngine::Cell(int row, int col) const {
  return row * cols_ + col;
}

int YottCoreRepairEngine::Row(int cell) const { return cell / cols_; }

int YottCoreRepairEngine::Col(int cell) const { return cell % cols_; }

bool YottCoreRepairEngine::IsCompatible(int dfg_node, int cell) const {
  if (cell < 0 || cell >= static_cast<int>(cell_to_mrrg_.size())) return false;
  if (cell_to_mrrg_[cell] < 0) return false;
  if (UsesStructuralIOCellTypes()) {
    return true;
  }
  const entity::OpType op = dfg_.GetNodeProperty(dfg_node).op;
  return std::find(cell_ops_[cell].begin(), cell_ops_[cell].end(), op) !=
         cell_ops_[cell].end();
}

bool YottCoreRepairEngine::IsIONode(int dfg_node) const {
  if (io_node_policy_ == "structural") {
    return predecessors_[dfg_node].empty() || successors_[dfg_node].empty();
  }
  const entity::OpType op = dfg_.GetNodeProperty(dfg_node).op;
  return entity::IsMemoryAccessOperation(op);
}

int YottCoreRepairEngine::CPUMappingCellType(int cell) const {
  if (cell < 0 || cell >= static_cast<int>(cell_to_mrrg_.size())) return -1;
  if (cell_to_mrrg_[cell] < 0) return -1;
  const int row = Row(cell);
  const int col = Col(cell);
  const bool is_corner =
      (row == 0 || row == rows_ - 1) && (col == 0 || col == cols_ - 1);
  if (separate_io_cells_) {
    // Structural-I/O YOTO/YOTT uses a logical type matrix: perimeter cells
    // host source/sink-like nodes and interior cells host compute nodes.
    // `perimeter_no_corners` still creates corner MRRG nodes, but the
    // paper-style grid treats those corner sites as unavailable.
    if (is_corner && !cell_memory_accessible_[cell]) return -1;
    return cell_memory_accessible_[cell] ? 1 : 0;
  }
  if (is_corner) return -1;
  const bool is_perimeter =
      row == 0 || col == 0 || row == rows_ - 1 || col == cols_ - 1;
  return is_perimeter ? 1 : 0;
}

bool YottCoreRepairEngine::IsCPUMappingCompatible(int dfg_node,
                                                  int cell) const {
  if (!IsCompatible(dfg_node, cell)) return false;
  const int cell_type = CPUMappingCellType(cell);
  if (cell_type < 0) return false;
  return cell_type == (IsIONode(dfg_node) ? 1 : 0);
}

bool YottCoreRepairEngine::CanPlaceCPUMapping(
    int dfg_node, int cell, const PlacementState& state) const {
  return cell >= 0 && cell < static_cast<int>(state.cell_to_dfg.size()) &&
         state.cell_to_dfg[cell] < 0 && IsCPUMappingCompatible(dfg_node, cell);
}

int YottCoreRepairEngine::CellFreedom(int cell) const {
  int result = 0;
  const int radius =
      config_.network_type == entity::MRRGNetworkType::kOneHop2d ? 2 : 1;
  const int row = Row(cell);
  const int col = Col(cell);
  for (int dr = -radius; dr <= radius; dr++) {
    for (int dc = -radius; dc <= radius; dc++) {
      if (dr == 0 && dc == 0) continue;
      const int other_row = row + dr;
      const int other_col = col + dc;
      if (other_row < 0 || other_col < 0 || other_row >= rows_ ||
          other_col >= cols_) {
        continue;
      }
      const int other = Cell(other_row, other_col);
      if (cell_to_mrrg_[other] < 0) continue;
      if (DistanceCost(cell, other) == 1) result++;
    }
  }
  return result;
}

std::vector<int> YottCoreRepairEngine::InitialFreedomGrid() const {
  return initial_freedom_;
}

int YottCoreRepairEngine::DistanceCost(int a, int b) const {
  const int dx = std::abs(Row(a) - Row(b));
  const int dy = std::abs(Col(a) - Col(b));
  if (config_.network_type == entity::MRRGNetworkType::kOneHop2d) {
    return std::max(1, (dx + 1) / 2 + (dy + 1) / 2);
  }
  if (config_.network_type == entity::MRRGNetworkType::kDiagonal) {
    return std::max(1, std::max(dx, dy));
  }
  return std::max(1, dx + dy);
}

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

std::string YottCoreRepairEngine::MapperName() const {
  switch (kind_) {
    case mapper::yott_core_repair::Placement2DArrayKind::
        kCPUMappingYOTTCoreRepair:
      return "Placement2DCPUMappingYOTTCoreRepair";
  }
  return "Placement2DArray";
}

bool YottCoreRepairEngine::UsesYOTTAnnotations() const {
  if (use_yott_annotations_.has_value()) {
    return use_yott_annotations_.value();
  }
  return false || true;
}

bool YottCoreRepairEngine::UsesStructuralIOCellTypes() const {
  return io_node_policy_ == "structural" && (true || false);
}

int YottCoreRepairEngine::MaxTrials() const {
  if (max_trials_.has_value()) return std::max(1, max_trials_.value());
  return std::max(100, dfg_.GetNodeNum() * 4);
}

int YottCoreRepairEngine::SeedCount() const {
  if (seed_count_.has_value()) return std::max(1, seed_count_.value());
  return 1;
}

int YottCoreRepairEngine::MaxIterations() const {
  if (max_iterations_.has_value()) {
    return std::max(1, max_iterations_.value());
  }
  return std::max(5000, dfg_.GetNodeNum() * rows_ * cols_ * 20);
}

int YottCoreRepairEngine::ElitePlacementCount() const {
  if (elite_placement_count_.has_value()) {
    return std::max(1, elite_placement_count_.value());
  }
  return 8;
}

unsigned int YottCoreRepairEngine::SeedFor(int seed_index) const {
  const unsigned int seed =
      random_seed_.has_value() ? static_cast<unsigned int>(random_seed_.value())
                               : base_seed_;
  return seed + static_cast<unsigned int>(seed_index * 0x9E3779B9u);
}

unsigned int YottCoreRepairEngine::TrialSeedFor(int seed_index,
                                                int trial_index) const {
  unsigned int seed = SeedFor(seed_index);
  seed ^= static_cast<unsigned int>(trial_index + 1) * 0x85EBCA6Bu;
  seed ^= seed >> 16;
  seed *= 0x7FEB352Du;
  seed ^= seed >> 15;
  return seed;
}

void YottCoreRepairEngine::ResetSeed(int seed_index) {
  const unsigned int seed = SeedFor(seed_index);
  rng_.seed(seed);
  std::srand(seed);
}

void YottCoreRepairEngine::ResetTrialSeed(int seed_index, int trial_index) {
  const unsigned int seed = TrialSeedFor(seed_index, trial_index);
  rng_.seed(seed);
  std::srand(seed);
}

bool YottCoreRepairEngine::UsesPerTrialSeeds() const {
  return trial_seed_policy_ == "per_trial";
}

int YottCoreRepairEngine::AuthorRandomInt(int exclusive_upper_bound) {
  if (exclusive_upper_bound <= 1) return 0;
  return std::rand() % exclusive_upper_bound;
}

void YottCoreRepairEngine::ShuffleWithAuthorRandom(std::vector<int>& values) {
  for (int i = 1; i < static_cast<int>(values.size()); i++) {
    std::swap(values[i], values[AuthorRandomInt(i + 1)]);
  }
}

void YottCoreRepairEngine::ShuffleWithAuthorRandom(
    std::array<std::pair<int, int>, 4>& values) {
  for (int i = 1; i < static_cast<int>(values.size()); i++) {
    std::swap(values[i], values[AuthorRandomInt(i + 1)]);
  }
}

int YottCoreRepairEngine::TraversalNeighborMode(bool is_yott) {
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
  if (true) return AuthorRandomInt(4);
  std::uniform_int_distribution<int> mode_dist(0, 3);
  return mode_dist(rng_);
}

bool YottCoreRepairEngine::HasTimedOut(
    std::chrono::steady_clock::time_point start, double reserve_s) const {
  return SecondsSince(start) + reserve_s >= timeout_s_;
}

void YottCoreRepairEngine::Log(const std::string& message) const {
  if (!log_file_path_.has_value()) return;
  std::ofstream ofs(log_file_path_.value(), std::ios::app);
  if (ofs) ofs << "[" << MapperName() << "] " << message << "\n";
}

bool YottCoreRepairEngine::TraceTrials() const {
  return trace_trials_.value_or(false) && log_file_path_.has_value();
}

std::string YottCoreRepairEngine::TrialTracePath() const {
  if (!log_file_path_.has_value()) return "trial_trace.csv";
  const std::string path = log_file_path_.value();
  const std::string::size_type slash = path.find_last_of("/\\");
  if (slash == std::string::npos) return "trial_trace.csv";
  return path.substr(0, slash + 1) + "trial_trace.csv";
}

void YottCoreRepairEngine::WriteTrialTraceHeader(std::ofstream& trace) const {
  trace << "mapper,phase,seed_index,trial_index,success,selected_as_best,"
           "selection_primary,selection_secondary,placement_cost,"
           "mesh_hop_sum,avg_mesh_hop,max_mesh_hop,"
           "mesh_optimal_edge_ratio,paper_optimal_edge_ratio,"
           "avg_mesh_fifo,max_mesh_fifo,p95_mesh_fifo,"
           "avg_paper_fifo,max_paper_fifo,elapsed_sec,trial_seed,"
           "plan_hash,placement_hash,placement_signature\n";
}

TrialTraceMetrics YottCoreRepairEngine::ComputeTrialTraceMetrics(
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

std::string YottCoreRepairEngine::PlacementSignature(
    const PlacementState& state) const {
  std::ostringstream out;
  for (int node = 0; node < static_cast<int>(state.dfg_to_cell.size());
       node++) {
    if (node > 0) out << ':';
    out << state.dfg_to_cell[node];
  }
  return out.str();
}

std::string YottCoreRepairEngine::PlacementHash(
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

std::string YottCoreRepairEngine::PlanHash(
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

void YottCoreRepairEngine::WriteTrialTraceRow(
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

void YottCoreRepairEngine::RecordPlacementSwapAttempts(long long count) const {
  if (count > 0) placement_swap_attempts_ += count;
}

std::string YottCoreRepairEngine::NodeLabel(int node) const {
  if (node < 0 || node >= dfg_.GetNodeNum()) return std::to_string(node);
  const auto property = dfg_.GetNodeProperty(node);
  return std::to_string(node) + ":" + property.op_name + "/" + property.op_str;
}

int YottCoreRepairEngine::PlacedCount(const PlacementState& state) const {
  int count = 0;
  for (int cell : state.dfg_to_cell) {
    if (cell >= 0) count++;
  }
  return count;
}

int YottCoreRepairEngine::FreeCPUMappingCompatibleCellCount(
    int dfg_node, const PlacementState& state) const {
  int count = 0;
  for (int cell : compatible_cells_[dfg_node]) {
    if (CanPlaceCPUMapping(dfg_node, cell, state)) count++;
  }
  return count;
}

void YottCoreRepairEngine::RecordFailure(const std::string& reason) {
  if (last_failure_reason_.empty()) last_failure_reason_ = reason;
}

double YottCoreRepairEngine::PlacementCost(const PlacementState& state) const {
  double cost = 0.0;
  for (const auto& edge : edges_) {
    const int from = state.dfg_to_cell[edge.source];
    const int to = state.dfg_to_cell[edge.target];
    if (from < 0 || to < 0) return kImpossibleCost;
    cost += DistanceCost(from, to);
  }
  return cost;
}

CostAwarePRISAMetrics YottCoreRepairEngine::ComputeCostAwarePRISAMetrics(
    const PlacementState& state) const {
  CostAwarePRISAMetrics metrics;
  const int node_count = dfg_.GetNodeNum();
  std::vector<int> indegree(node_count, 0);
  std::vector<std::vector<std::pair<int, int>>> weighted_successors(node_count);

  for (const auto& edge : edges_) {
    const int from = state.dfg_to_cell[edge.source];
    const int to = state.dfg_to_cell[edge.target];
    if (from < 0 || to < 0) {
      metrics.mesh_hop_sum = kImpossibleCost;
      metrics.max_mesh_hop = std::numeric_limits<int>::max();
      metrics.mapped_lp_mesh_hop = std::numeric_limits<int>::max();
      return metrics;
    }
    const int hop = DistanceCost(from, to);
    metrics.mesh_hop_sum += hop;
    metrics.max_mesh_hop = std::max(metrics.max_mesh_hop, hop);
    if (hop <= 1) metrics.direct_edge_count++;
    weighted_successors[edge.source].push_back({edge.target, std::max(1, hop)});
    indegree[edge.target]++;
  }

  std::queue<int> q;
  std::vector<int> distance(node_count, 0);
  for (int node = 0; node < node_count; node++) {
    if (indegree[node] == 0) q.push(node);
  }

  int visited = 0;
  while (!q.empty()) {
    const int source = q.front();
    q.pop();
    visited++;
    metrics.mapped_lp_mesh_hop =
        std::max(metrics.mapped_lp_mesh_hop, distance[source]);
    for (const auto& [target, weight] : weighted_successors[source]) {
      distance[target] = std::max(distance[target], distance[source] + weight);
      indegree[target]--;
      if (indegree[target] == 0) q.push(target);
    }
  }

  if (visited != node_count) {
    metrics.mapped_lp_mesh_hop = static_cast<int>(metrics.mesh_hop_sum);
  } else {
    for (int value : distance) {
      metrics.mapped_lp_mesh_hop = std::max(metrics.mapped_lp_mesh_hop, value);
    }
  }
  return metrics;
}

void YottCoreRepairEngine::PlaceNode(int dfg_node, int cell,
                                     PlacementState& state) const {
  state.dfg_to_cell[dfg_node] = cell;
  state.cell_to_dfg[cell] = dfg_node;
}

// Shared traversal graph utilities used by paper-guided and cpu_mapping
// YOTO/YOTT variants. Method-specific files decide how these scores are used.

bool YottCoreRepairEngine::IsOutputNode(int dfg_node) const {
  return dfg_.GetNodeProperty(dfg_node).op == entity::OpType::kOutput;
}

std::vector<int> YottCoreRepairEngine::CPUMappingTraversalRoots() {
  std::vector<int> roots;
  for (int node = 0; node < dfg_.GetNodeNum(); node++) {
    if (successors_[node].empty() || IsOutputNode(node)) roots.push_back(node);
  }
  if (roots.empty()) {
    roots.resize(dfg_.GetNodeNum());
    std::iota(roots.begin(), roots.end(), 0);
  }
  ShuffleWithAuthorRandom(roots);
  return roots;
}

std::vector<int> YottCoreRepairEngine::BuildCriticalPathScore() const {
  const int n = dfg_.GetNodeNum();
  std::vector<int> memo(n, -1);
  std::vector<char> visiting(n, false);
  std::function<int(int)> dfs = [&](int node) {
    if (memo[node] >= 0) return memo[node];
    if (visiting[node]) return 0;
    visiting[node] = true;
    int best = 0;
    for (int successor : successors_[node]) {
      if (successor == node) continue;
      best = std::max(best, 1 + dfs(successor));
    }
    visiting[node] = false;
    memo[node] = best;
    return best;
  };
  for (int node = 0; node < n; node++) dfs(node);
  return memo;
}

std::vector<int> YottCoreRepairEngine::BuildCPUMappingInputDistanceScore()
    const {
  const int n = dfg_.GetNodeNum();
  std::vector<int> distance(n, -1);
  std::queue<int> q;
  for (int node = 0; node < n; node++) {
    if (!predecessors_[node].empty()) continue;
    distance[node] = 0;
    q.push(node);
  }
  if (q.empty()) {
    for (int node = 0; node < n; node++) {
      distance[node] = 0;
      q.push(node);
    }
  }
  while (!q.empty()) {
    const int node = q.front();
    q.pop();
    for (int successor : successors_[node]) {
      if (successor == node) continue;
      const int next_distance = distance[node] + 1;
      if (next_distance <= distance[successor]) continue;
      distance[successor] = next_distance;
      q.push(successor);
    }
  }
  for (int& value : distance) {
    if (value < 0) value = 0;
  }
  return distance;
}

std::vector<double> YottCoreRepairEngine::BuildBetweennessCentralityScore()
    const {
  const int n = dfg_.GetNodeNum();
  std::vector<double> centrality(n, 0.0);
  for (int source = 0; source < n; source++) {
    std::vector<std::vector<int>> predecessors_on_paths(n);
    std::vector<int> distance(n, -1);
    std::vector<double> sigma(n, 0.0);
    std::vector<double> dependency(n, 0.0);
    std::vector<int> order;
    std::queue<int> q;

    distance[source] = 0;
    sigma[source] = 1.0;
    q.push(source);
    while (!q.empty()) {
      const int node = q.front();
      q.pop();
      order.push_back(node);
      for (int successor : successors_[node]) {
        if (successor == node) continue;
        if (distance[successor] < 0) {
          distance[successor] = distance[node] + 1;
          q.push(successor);
        }
        if (distance[successor] == distance[node] + 1) {
          sigma[successor] += sigma[node];
          predecessors_on_paths[successor].push_back(node);
        }
      }
    }

    for (auto it = order.rbegin(); it != order.rend(); ++it) {
      const int node = *it;
      if (sigma[node] == 0.0) continue;
      for (int predecessor : predecessors_on_paths[node]) {
        dependency[predecessor] +=
            (sigma[predecessor] / sigma[node]) * (1.0 + dependency[node]);
      }
      if (node != source) centrality[node] += dependency[node];
    }
  }
  return centrality;
}

}  // namespace mapper::yott_core_repair
