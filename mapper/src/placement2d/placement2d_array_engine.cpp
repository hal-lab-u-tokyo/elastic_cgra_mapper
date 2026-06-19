#include <mapper/placement2d/placement2d_array_mapper_base.hpp>

// Implementation detail for the direct 2D-grid placement fast path. Public
// mapper classes and registrations live in the method-specific files:
// placement2d_yoto_mapper.cpp, placement2d_yott_mapper.cpp,
// placement2d_prisa_mapper.cpp, and placement2d_sa_mapper.cpp.
//
// Reading guide:
// 1. Run() dispatches by Placement2DArrayKind.
// 2. Cache builders convert DFG/MRRG data into compact arrays.
// 3. Shared scoring helpers define the placement objective.
// 4. Traversal/cpu_mapping sections implement YOTO/YOTT-style placement.
// 5. PRISA sections implement SIS, weak/potential regions, and swap moves.
// 6. The final SA section is the internal simulated annealing baseline.

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <fstream>
#include <functional>
#include <limits>
#include <numeric>
#include <queue>
#include <random>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr double kImpossibleCost = 1.0e12;

struct EdgeInfo {
  int source = -1;
  int target = -1;
};

struct PlacementState {
  std::vector<int> dfg_to_cell;
  std::vector<int> cell_to_dfg;
};

struct PRISAStats {
  std::vector<int> row_weak;
  std::vector<int> column_weak;
  std::vector<int> row_potential;
  std::vector<int> column_potential;
  std::vector<char> edge_weak;
  std::vector<char> edge_potential;
  int weak_edges = 0;
};

struct PlacementQuality {
  double cost = kImpossibleCost;
  int direct_edges = 0;
  int max_distance = 0;
};

struct CostAwarePRISAMetrics {
  double mesh_hop_sum = 0.0;
  int max_mesh_hop = 0;
  int mapped_lp_mesh_hop = 0;
  int direct_edge_count = 0;
};

struct Step {
  int anchor = -1;
  int target = -1;
};

struct CellScore {
  std::array<double, 3> value = {
      std::numeric_limits<double>::infinity(),
      std::numeric_limits<double>::infinity(),
      std::numeric_limits<double>::infinity()};
};

double SecondsSince(std::chrono::steady_clock::time_point start) {
  const auto now = std::chrono::steady_clock::now();
  return std::chrono::duration_cast<std::chrono::duration<double>>(now - start)
      .count();
}

class Placement2DArrayEngine {
 public:
  Placement2DArrayEngine(entity::DFG& dfg, entity::MRRG& mrrg,
                         mapper::Placement2DArrayKind kind, double timeout_s,
                         const std::optional<std::string>& log_file_path,
                         std::optional<int> max_trials,
                         std::optional<int> seed_count,
                         std::optional<int> random_seed,
                         std::optional<int> max_iterations)
      : dfg_(dfg),
        mrrg_(mrrg),
        kind_(kind),
        timeout_s_(timeout_s > 0.0 ? timeout_s : 1.0),
        log_file_path_(log_file_path),
        max_trials_(max_trials),
        seed_count_(seed_count),
        random_seed_(random_seed),
        max_iterations_(max_iterations),
        config_(mrrg.GetMRRGConfig()),
        base_seed_(static_cast<unsigned int>(
            0x85EBCA6Bu ^
            static_cast<unsigned int>(dfg.GetNodeNum() * 131 +
                                      config_.row * 17 + config_.column))),
        rng_(base_seed_) {
    BuildDFGCache();
    BuildGridCache();
    BuildCompatibilityCache();
    BuildPRISACache();
  }

  mapper::MappingResult Run() {
    const auto start = std::chrono::steady_clock::now();
    Log("start mapper=" + MapperName() + " nodes=" +
        std::to_string(dfg_.GetNodeNum()) + " cells=" +
        std::to_string(rows_ * cols_) + " max_trials=" +
        std::to_string(MaxTrials()) + " seed_count=" +
        std::to_string(SeedCount()));

    std::optional<PlacementState> placement;
    if (IsPRISALike()) {
      placement = RunPRISAMultiSeed(start);
    } else if (kind_ == mapper::Placement2DArrayKind::kSA) {
      placement = RunSAMultiSeed(start);
    } else if (IsCPUMappingLike()) {
      placement = RunCPUMappingMultiStart(start);
    } else {
      placement = RunTraversalMultiStart(start);
    }

    if (!placement.has_value()) {
      return mapper::MappingResult(false, entity::Mapping(config_),
                                   SecondsSince(start));
    }

    std::vector<int> dfg_to_mrrg(dfg_.GetNodeNum(), -1);
    for (int node = 0; node < dfg_.GetNodeNum(); node++) {
      const int cell = placement->dfg_to_cell[node];
      if (cell < 0 || cell >= static_cast<int>(cell_to_mrrg_.size())) {
        return mapper::MappingResult(false, entity::Mapping(config_),
                                     SecondsSince(start));
      }
      dfg_to_mrrg[node] = cell_to_mrrg_[cell];
    }

    std::vector<std::vector<int>> empty_routes(dfg_.GetNodeNum());
    entity::Mapping mapping(mrrg_, dfg_, dfg_to_mrrg, empty_routes);
    const double elapsed = SecondsSince(start);
    Log("success mapper=" + MapperName() + " time_s=" +
        std::to_string(elapsed) + " placement_cost=" +
        std::to_string(PlacementCost(*placement)) + " cell_visits=" +
        std::to_string(cell_visits_));
    return mapper::MappingResult(true, mapping, elapsed);
  }

 private:
  entity::DFG& dfg_;
  entity::MRRG& mrrg_;
  mapper::Placement2DArrayKind kind_;
  double timeout_s_;
  std::optional<std::string> log_file_path_;
  std::optional<int> max_trials_;
  std::optional<int> seed_count_;
  std::optional<int> random_seed_;
  std::optional<int> max_iterations_;
  entity::MRRGConfig config_;
  unsigned int base_seed_;
  std::mt19937 rng_;

  int rows_ = 0;
  int cols_ = 0;
  std::vector<int> cell_to_mrrg_;
  std::vector<std::vector<entity::OpType>> cell_ops_;
  std::vector<bool> cell_memory_accessible_;
  std::vector<EdgeInfo> edges_;
  std::vector<std::vector<int>> successors_;
  std::vector<std::vector<int>> predecessors_;
  std::vector<std::vector<int>> incident_edge_ids_;
  std::vector<int> degree_;
  std::vector<std::vector<int>> compatible_cells_;
  std::vector<int> prisa_cell_order_;
  std::vector<int> prisa_order_index_;
  std::vector<int> prisa_distance_matrix_;
  std::vector<char> prisa_weak_region_matrix_;
  int prisa_weak_distance_threshold_ = std::numeric_limits<int>::max();
  bool separate_io_cells_ = false;
  long long cell_visits_ = 0;

  // Runtime options and mapper-family dispatch.
  std::string MapperName() const {
    switch (kind_) {
      case mapper::Placement2DArrayKind::kYOTO:
        return "Placement2DArrayYOTO";
      case mapper::Placement2DArrayKind::kYOTT:
        return "Placement2DArrayYOTT";
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

  bool IsPRISALike() const {
    return kind_ == mapper::Placement2DArrayKind::kPRISA ||
           kind_ == mapper::Placement2DArrayKind::kPRISANoSIS ||
           kind_ == mapper::Placement2DArrayKind::kCostAwarePRISA;
  }

  bool UsesPRISASIS() const {
    return kind_ == mapper::Placement2DArrayKind::kPRISA ||
           kind_ == mapper::Placement2DArrayKind::kCostAwarePRISA;
  }

  bool UsesCostAwarePRISA() const {
    return kind_ == mapper::Placement2DArrayKind::kCostAwarePRISA;
  }

  bool IsCPUMappingLike() const {
    return kind_ == mapper::Placement2DArrayKind::kCPUMappingYOTO ||
           kind_ == mapper::Placement2DArrayKind::kCPUMappingYOTT;
  }

  bool IsCPUMappingYOTO() const {
    return kind_ == mapper::Placement2DArrayKind::kCPUMappingYOTO;
  }

  bool UsesYOTOTraversalOrder() const {
    return kind_ == mapper::Placement2DArrayKind::kYOTO ||
           kind_ == mapper::Placement2DArrayKind::kCPUMappingYOTO;
  }

  int MaxTrials() const {
    if (max_trials_.has_value()) return std::max(1, max_trials_.value());
    return std::max(100, dfg_.GetNodeNum() * 4);
  }

  int SeedCount() const {
    if (seed_count_.has_value()) return std::max(1, seed_count_.value());
    return 1;
  }

  int MaxIterations() const {
    if (max_iterations_.has_value()) {
      return std::max(1, max_iterations_.value());
    }
    return std::max(5000, dfg_.GetNodeNum() * rows_ * cols_ * 20);
  }

  unsigned int SeedFor(int seed_index) const {
    const unsigned int seed = random_seed_.has_value()
                                  ? static_cast<unsigned int>(random_seed_.value())
                                  : base_seed_;
    return seed + static_cast<unsigned int>(seed_index * 0x9E3779B9u);
  }

  void ResetSeed(int seed_index) { rng_.seed(SeedFor(seed_index)); }

  bool HasTimedOut(std::chrono::steady_clock::time_point start,
                   double reserve_s = 0.01) const {
    return SecondsSince(start) + reserve_s >= timeout_s_;
  }

  void Log(const std::string& message) const {
    if (!log_file_path_.has_value()) return;
    std::ofstream ofs(log_file_path_.value(), std::ios::app);
    if (ofs) ofs << "[" << MapperName() << "] " << message << "\n";
  }

  // Input/cache construction.
  void BuildDFGCache() {
    const int node_num = dfg_.GetNodeNum();
    successors_.assign(node_num, {});
    predecessors_.assign(node_num, {});
    incident_edge_ids_.assign(node_num, {});
    degree_.assign(node_num, 0);
    for (int source = 0; source < node_num; source++) {
      successors_[source] = dfg_.GetAdjacentNodeIdVec(source);
      for (int target : successors_[source]) {
        const int edge_id = static_cast<int>(edges_.size());
        edges_.push_back({source, target});
        if (source >= 0 && source < node_num) {
          incident_edge_ids_[source].push_back(edge_id);
        }
        if (target >= 0 && target < node_num && target != source) {
          predecessors_[target].push_back(source);
          incident_edge_ids_[target].push_back(edge_id);
        }
      }
    }
    for (int node = 0; node < node_num; node++) {
      degree_[node] = static_cast<int>(successors_[node].size() +
                                       predecessors_[node].size());
    }
  }

  void BuildGridCache() {
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
  }

  void BuildCompatibilityCache() {
    compatible_cells_.assign(dfg_.GetNodeNum(), {});
    for (int node = 0; node < dfg_.GetNodeNum(); node++) {
      auto& cells = compatible_cells_[node];
      cells.reserve(cell_to_mrrg_.size());
      for (int cell = 0; cell < static_cast<int>(cell_to_mrrg_.size());
           cell++) {
        if (IsCompatible(node, cell)) cells.push_back(cell);
      }
    }
  }

  int PRISAResourceSideLength() const { return std::max(rows_, cols_); }

  int PRISAWeakPairCount(int resource_count) const {
    const int k = std::max(1, PRISAResourceSideLength());
    if (k < 2 || resource_count < 2) return 0;
    const long long kk = k;
    const long long numerator =
        kk * kk * kk * kk + 2 * kk * kk * kk - kk * kk - 2 * kk;
    const long long paper_weak_pairs = numerator / 12;
    const long long total_pairs =
        static_cast<long long>(resource_count) * (resource_count - 1) / 2;
    return static_cast<int>(
        std::max(0LL, std::min(paper_weak_pairs, total_pairs)));
  }

  void BuildPRISACache() {
    prisa_cell_order_.clear();
    prisa_cell_order_.reserve(rows_ * cols_);
    for (int row = 0; row < rows_; row++) {
      if ((row % 2) == 0) {
        for (int col = 0; col < cols_; col++) {
          const int cell = Cell(row, col);
          if (cell_to_mrrg_[cell] >= 0) prisa_cell_order_.push_back(cell);
        }
      } else {
        for (int col = cols_ - 1; col >= 0; col--) {
          const int cell = Cell(row, col);
          if (cell_to_mrrg_[cell] >= 0) prisa_cell_order_.push_back(cell);
        }
      }
    }

    prisa_order_index_.assign(rows_ * cols_, -1);
    for (int i = 0; i < static_cast<int>(prisa_cell_order_.size()); i++) {
      prisa_order_index_[prisa_cell_order_[i]] = i;
    }

    const int resource_count = static_cast<int>(prisa_cell_order_.size());
    prisa_distance_matrix_.assign(resource_count * resource_count, 0);
    prisa_weak_region_matrix_.assign(resource_count * resource_count, 0);
    prisa_weak_distance_threshold_ = std::numeric_limits<int>::max();
    if (resource_count < 2) return;

    for (int row = 0; row < resource_count; row++) {
      for (int column = 0; column < resource_count; column++) {
        if (row == column) continue;
        const int distance =
            DistanceCost(prisa_cell_order_[row], prisa_cell_order_[column]);
        prisa_distance_matrix_[row * resource_count + column] = distance;
      }
    }

    std::vector<int> pair_distances;
    pair_distances.reserve(resource_count * (resource_count - 1) / 2);
    for (int row = 0; row < resource_count; row++) {
      for (int column = 0; column < resource_count; column++) {
        if (row == column) continue;
        if (row < column) {
          pair_distances.push_back(
              prisa_distance_matrix_[row * resource_count + column]);
        }
      }
    }

    const int weak_pair_count = PRISAWeakPairCount(resource_count);
    if (weak_pair_count <= 0 || pair_distances.empty()) return;
    std::sort(pair_distances.begin(), pair_distances.end(),
              std::greater<int>());
    const int threshold_index =
        std::min(weak_pair_count, static_cast<int>(pair_distances.size())) - 1;
    prisa_weak_distance_threshold_ = pair_distances[threshold_index];
    for (int row = 0; row < resource_count; row++) {
      for (int column = 0; column < resource_count; column++) {
        if (row == column) continue;
        // The paper separates the distance matrix into heavy weak entries and
        // light potential entries.  On a mesh, many cells share the same
        // distance, so the threshold intentionally includes ties at the weak
        // boundary rather than imposing an arbitrary cell ordering.
        prisa_weak_region_matrix_[row * resource_count + column] =
            prisa_distance_matrix_[row * resource_count + column] >=
            prisa_weak_distance_threshold_;
      }
    }
  }

  // Grid, compatibility, and architecture-cost helpers.
  int Cell(int row, int col) const { return row * cols_ + col; }
  int Row(int cell) const { return cell / cols_; }
  int Col(int cell) const { return cell % cols_; }

  bool IsCompatible(int dfg_node, int cell) const {
    if (cell < 0 || cell >= static_cast<int>(cell_to_mrrg_.size())) return false;
    if (cell_to_mrrg_[cell] < 0) return false;
    const entity::OpType op = dfg_.GetNodeProperty(dfg_node).op;
    return std::find(cell_ops_[cell].begin(), cell_ops_[cell].end(), op) !=
           cell_ops_[cell].end();
  }

  bool IsIONode(int dfg_node) const {
    const entity::OpType op = dfg_.GetNodeProperty(dfg_node).op;
    return entity::IsMemoryAccessOperation(op);
  }

  bool CanPlace(int dfg_node, int cell, const PlacementState& state) const {
    return state.cell_to_dfg[cell] < 0 && IsCompatible(dfg_node, cell);
  }

  bool IsCPUMappingCompatible(int dfg_node, int cell) const {
    if (!IsCompatible(dfg_node, cell)) return false;
    if (!separate_io_cells_) return true;
    // Perimeter I/O PEs still support ordinary compute operations in the MRRG.
    // Only memory operations must be restricted to memory-accessible cells.
    if (IsIONode(dfg_node)) return cell_memory_accessible_[cell];
    return true;
  }

  bool CanPlaceCPUMapping(int dfg_node, int cell,
                          const PlacementState& state) const {
    return cell >= 0 && cell < static_cast<int>(state.cell_to_dfg.size()) &&
           state.cell_to_dfg[cell] < 0 &&
           IsCPUMappingCompatible(dfg_node, cell);
  }

  int OffsetOrder(int anchor, int cell) const {
    const int dr = Row(cell) - Row(anchor);
    const int dc = Col(cell) - Col(anchor);
    if (dr == 0 && dc > 0) return dc * 16 + 0;
    if (dr > 0 && dc == 0) return dr * 16 + 1;
    if (dr == 0 && dc < 0) return (-dc) * 16 + 2;
    if (dr < 0 && dc == 0) return (-dr) * 16 + 3;
    const int shell = std::abs(dr) + std::abs(dc);
    int quadrant = 4;
    if (dr > 0 && dc > 0) quadrant = 4;
    else if (dr > 0 && dc < 0) quadrant = 5;
    else if (dr < 0 && dc < 0) quadrant = 6;
    else if (dr < 0 && dc > 0) quadrant = 7;
    return shell * 16 + quadrant;
  }

  int CellFreedom(int cell) const {
    int result = 0;
    for (int other = 0; other < rows_ * cols_; other++) {
      if (other == cell || cell_to_mrrg_[other] < 0) continue;
      if (DistanceCost(cell, other) == 1) result++;
    }
    return result;
  }

  std::vector<int> InitialFreedomGrid() const {
    std::vector<int> freedom(rows_ * cols_, 0);
    for (int cell = 0; cell < rows_ * cols_; cell++) {
      if (cell_to_mrrg_[cell] >= 0) freedom[cell] = CellFreedom(cell);
    }
    return freedom;
  }

  void UpdateFreedomGrid(int placed_cell, std::vector<int>& freedom) const {
    if (placed_cell < 0 || placed_cell >= static_cast<int>(freedom.size())) {
      return;
    }
    freedom[placed_cell] = 0;
    for (int cell = 0; cell < static_cast<int>(freedom.size()); cell++) {
      if (freedom[cell] <= 1) continue;
      if (DistanceCost(placed_cell, cell) == 1) freedom[cell]--;
    }
  }

  int DistanceCost(int a, int b) const {
    const int dx = std::abs(Row(a) - Row(b));
    const int dy = std::abs(Col(a) - Col(b));
    if (config_.network_type == entity::MRRGNetworkType::kOneHopAxis2) {
      return std::max(1, (dx + 1) / 2 + (dy + 1) / 2);
    }
    if (config_.network_type == entity::MRRGNetworkType::kDiagonal) {
      return std::max(1, std::max(dx, dy));
    }
    return std::max(1, dx + dy);
  }

  // Shared placement-quality helpers used to compare candidates across methods.
  double PlacementCost(const PlacementState& state) const {
    double cost = 0.0;
    for (const auto& edge : edges_) {
      const int from = state.dfg_to_cell[edge.source];
      const int to = state.dfg_to_cell[edge.target];
      if (from < 0 || to < 0) return kImpossibleCost;
      cost += DistanceCost(from, to);
    }
    return cost;
  }

  PlacementQuality ComputePlacementQuality(const PlacementState& state) const {
    PlacementQuality quality;
    quality.cost = 0.0;
    quality.direct_edges = 0;
    quality.max_distance = 0;
    for (const auto& edge : edges_) {
      const int from = state.dfg_to_cell[edge.source];
      const int to = state.dfg_to_cell[edge.target];
      if (from < 0 || to < 0) {
        quality.cost = kImpossibleCost;
        return quality;
      }
      const int distance = DistanceCost(from, to);
      quality.cost += distance;
      if (distance <= 1) quality.direct_edges++;
      quality.max_distance = std::max(quality.max_distance, distance);
    }
    return quality;
  }

  bool IsBetterQuality(const PlacementQuality& candidate,
                       const PlacementQuality& best) const {
    if (candidate.cost + 1.0e-9 < best.cost) return true;
    if (candidate.cost > best.cost + 1.0e-9) return false;
    if (candidate.direct_edges != best.direct_edges &&
        candidate.max_distance <= best.max_distance) {
      return candidate.direct_edges > best.direct_edges;
    }
    if (candidate.max_distance != best.max_distance) {
      return candidate.max_distance < best.max_distance;
    }
    return candidate.direct_edges > best.direct_edges;
  }

  int PRISAWeakEdgeCount(const PlacementState& state) const {
    int result = 0;
    for (int edge_id = 0; edge_id < static_cast<int>(edges_.size());
         edge_id++) {
      bool is_weak = false;
      bool is_potential = false;
      if (PRISAEdgeRegion(state, edge_id, &is_weak, &is_potential) &&
          is_weak) {
        result++;
      }
    }
    return result;
  }

  CostAwarePRISAMetrics ComputeCostAwarePRISAMetrics(
      const PlacementState& state) const {
    CostAwarePRISAMetrics metrics;
    const int node_count = dfg_.GetNodeNum();
    std::vector<int> indegree(node_count, 0);
    std::vector<std::vector<std::pair<int, int>>> weighted_successors(
        node_count);

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
      weighted_successors[edge.source].push_back(
          {edge.target, std::max(1, hop)});
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
        metrics.mapped_lp_mesh_hop =
            std::max(metrics.mapped_lp_mesh_hop, value);
      }
    }
    return metrics;
  }

  double CostAwarePRISAScore(const PlacementState& state) const {
    const int weak_edges = PRISAWeakEdgeCount(state);
    const CostAwarePRISAMetrics metrics = ComputeCostAwarePRISAMetrics(state);
    if (metrics.mesh_hop_sum >= kImpossibleCost ||
        metrics.max_mesh_hop == std::numeric_limits<int>::max() ||
        metrics.mapped_lp_mesh_hop == std::numeric_limits<int>::max()) {
      return kImpossibleCost;
    }
    return weak_edges * 1000000.0 + metrics.mesh_hop_sum * 50.0 +
           metrics.mapped_lp_mesh_hop * 100.0 +
           metrics.max_mesh_hop * 200.0 -
           metrics.direct_edge_count * 80.0;
  }

  double PRISAAcceptanceScore(const PlacementState& state) const {
    if (UsesCostAwarePRISA()) return CostAwarePRISAScore(state);
    return PlacementCost(state);
  }

  double IncrementalCost(int dfg_node, int cell,
                         const PlacementState& state) const {
    double cost = 0.0;
    int count = 0;
    for (int pred : predecessors_[dfg_node]) {
      const int pred_cell = state.dfg_to_cell[pred];
      if (pred_cell >= 0) {
        cost += DistanceCost(pred_cell, cell);
        count++;
      }
    }
    for (int succ : successors_[dfg_node]) {
      const int succ_cell = state.dfg_to_cell[succ];
      if (succ_cell >= 0) {
        cost += DistanceCost(cell, succ_cell);
        count++;
      }
    }
    return count > 0 ? cost : 0.0;
  }

  int BorderDistance(int cell) const {
    return std::min(std::min(Row(cell), rows_ - 1 - Row(cell)),
                    std::min(Col(cell), cols_ - 1 - Col(cell)));
  }

  double IOScore(int dfg_node, int cell) const {
    double score = BorderDistance(cell);
    if (entity::IsMemoryAccessOperation(dfg_.GetNodeProperty(dfg_node).op)) {
      score += cell_memory_accessible_[cell] ? -16.0 : 1000.0;
    }
    return score;
  }

  double ReservedIOSlotPenalty(int dfg_node, int cell) const {
    const entity::OpType op = dfg_.GetNodeProperty(dfg_node).op;
    if (entity::IsMemoryAccessOperation(op)) return 0.0;
    return cell_memory_accessible_[cell] ? 1024.0 : 0.0;
  }

  std::vector<int> CompatibleCells(int dfg_node,
                                   const PlacementState& state) const {
    std::vector<int> cells;
    cells.reserve(compatible_cells_[dfg_node].size());
    for (int cell : compatible_cells_[dfg_node]) {
      if (CanPlace(dfg_node, cell, state)) cells.push_back(cell);
    }
    return cells;
  }

  bool IsBetterScore(const CellScore& candidate, const CellScore& best) const {
    constexpr double kEps = 1.0e-9;
    for (size_t i = 0; i < candidate.value.size(); i++) {
      if (candidate.value[i] + kEps < best.value[i]) return true;
      if (candidate.value[i] > best.value[i] + kEps) return false;
    }
    return false;
  }

  bool IsEqualScore(const CellScore& lhs, const CellScore& rhs) const {
    constexpr double kEps = 1.0e-9;
    for (size_t i = 0; i < lhs.value.size(); i++) {
      if (std::abs(lhs.value[i] - rhs.value[i]) > kEps) return false;
    }
    return true;
  }

  int ChooseBestCell(int dfg_node, const PlacementState& state,
                     const std::function<CellScore(int)>& score_fn) {
    int best_cell = -1;
    int equal_best_count = 0;
    CellScore best_score;
    for (int cell : compatible_cells_[dfg_node]) {
      if (state.cell_to_dfg[cell] >= 0) continue;
      cell_visits_++;
      const CellScore score = score_fn(cell);
      if (best_cell < 0 || IsBetterScore(score, best_score)) {
        best_cell = cell;
        best_score = score;
        equal_best_count = 1;
      } else if (IsEqualScore(score, best_score)) {
        equal_best_count++;
        std::uniform_int_distribution<int> dist(0, equal_best_count - 1);
        if (dist(rng_) == 0) best_cell = cell;
      }
    }
    return best_cell;
  }

  int ChooseInitialCell(int dfg_node, PlacementState& state) {
    return ChooseBestCell(dfg_node, state, [&](int cell) {
      if (IsIONode(dfg_node)) {
        return CellScore{{IOScore(dfg_node, cell), 0.0, 0.0}};
      }
      const int center_row = rows_ / 2;
      const int center_col = cols_ / 2;
      const int center_distance =
          std::abs(Row(cell) - center_row) + std::abs(Col(cell) - center_col);
      return CellScore{{static_cast<double>(center_distance) +
                            ReservedIOSlotPenalty(dfg_node, cell),
                        0.0, 0.0}};
    });
  }

  void PlaceNode(int dfg_node, int cell, PlacementState& state) const {
    state.dfg_to_cell[dfg_node] = cell;
    state.cell_to_dfg[cell] = dfg_node;
  }

  // YOTO/YOTT-style greedy traversal. YOTO and YOTT share the same skeleton;
  // their differences are traversal priority and local cell scoring.
  std::vector<int> TraversalRoots() {
    std::vector<int> roots;
    for (int node = 0; node < dfg_.GetNodeNum(); node++) {
      if (successors_[node].empty() ||
          dfg_.GetNodeProperty(node).op == entity::OpType::OUTPUT) {
        roots.push_back(node);
      }
    }
    if (roots.empty()) {
      roots.resize(dfg_.GetNodeNum());
      std::iota(roots.begin(), roots.end(), 0);
    }
    std::shuffle(roots.begin(), roots.end(), rng_);
    std::stable_sort(roots.begin(), roots.end(), [&](int a, int b) {
      if (IsIONode(a) != IsIONode(b)) return IsIONode(a) > IsIONode(b);
      return degree_[a] > degree_[b];
    });
    return roots;
  }

  std::vector<Step> BuildTraversalPlan() {
    std::vector<Step> plan;
    std::vector<char> visited(dfg_.GetNodeNum(), false);
    std::vector<int> stack;
    auto push_component = [&]() {
      for (int node : TraversalRoots()) {
        if (!visited[node]) {
          stack.push_back(node);
          visited[node] = true;
          return true;
        }
      }
      for (int node = 0; node < dfg_.GetNodeNum(); node++) {
        if (!visited[node]) {
          stack.push_back(node);
          visited[node] = true;
          return true;
        }
      }
      return false;
    };

    push_component();
    while (!stack.empty()) {
      const int anchor = stack.back();
      stack.pop_back();
      std::vector<int> candidates;
      candidates.reserve(successors_[anchor].size() + predecessors_[anchor].size());
      for (int succ : successors_[anchor]) {
        if (!visited[succ]) candidates.push_back(succ);
      }
      for (int pred : predecessors_[anchor]) {
        if (!visited[pred]) candidates.push_back(pred);
      }
      std::shuffle(candidates.begin(), candidates.end(), rng_);
      if (UsesYOTOTraversalOrder()) {
        std::stable_sort(candidates.begin(), candidates.end(), [&](int a, int b) {
          return degree_[a] > degree_[b];
        });
      } else {
        std::stable_sort(candidates.begin(), candidates.end(), [&](int a, int b) {
          const int score_a =
              degree_[a] + static_cast<int>(predecessors_[a].size()) *
                               static_cast<int>(successors_[a].size());
          const int score_b =
              degree_[b] + static_cast<int>(predecessors_[b].size()) *
                               static_cast<int>(successors_[b].size());
          return score_a > score_b;
        });
      }
      for (int target : candidates) {
        visited[target] = true;
        plan.push_back({anchor, target});
        stack.push_back(target);
      }
      if (stack.empty()) push_component();
    }
    return plan;
  }

  // cpu_mapping-style YOTO/YOTT variants. They reuse the traversal plan but use
  // a local-neighborhood placement policy and explicit IO/compute cell split.
  int FindUnusedEdgeIndex(int source, int target,
                          const std::vector<char>& used_edges) const {
    for (int i = 0; i < static_cast<int>(edges_.size()); i++) {
      if (used_edges[i]) continue;
      if (edges_[i].source == source && edges_[i].target == target) return i;
    }
    return -1;
  }

  std::vector<Step> BuildCPUMappingPlan() {
    std::vector<Step> plan;
    std::vector<char> used_edges(edges_.size(), false);
    for (const auto& step : BuildTraversalPlan()) {
      plan.push_back(step);
      int edge_id = FindUnusedEdgeIndex(step.anchor, step.target, used_edges);
      if (edge_id < 0) {
        edge_id = FindUnusedEdgeIndex(step.target, step.anchor, used_edges);
      }
      if (edge_id >= 0) used_edges[edge_id] = true;
    }

    std::vector<int> remaining;
    for (int edge_id = 0; edge_id < static_cast<int>(edges_.size()); edge_id++) {
      if (!used_edges[edge_id]) remaining.push_back(edge_id);
    }
    std::shuffle(remaining.begin(), remaining.end(), rng_);
    for (int edge_id : remaining) {
      plan.push_back({edges_[edge_id].source, edges_[edge_id].target});
    }
    return plan;
  }

  int ChooseCPUMappingInitialCell(int dfg_node, const PlacementState& state) {
    std::vector<int> preferred;
    std::vector<int> fallback;
    for (int cell : compatible_cells_[dfg_node]) {
      if (!CanPlaceCPUMapping(dfg_node, cell, state)) continue;
      fallback.push_back(cell);
      if (!separate_io_cells_ || IsIONode(dfg_node) == cell_memory_accessible_[cell]) {
        preferred.push_back(cell);
      }
    }
    const auto& candidates = preferred.empty() ? fallback : preferred;
    if (candidates.empty()) return -1;
    std::uniform_int_distribution<int> dist(
        0, static_cast<int>(candidates.size()) - 1);
    return candidates[dist(rng_)];
  }

  int CPUMappingFreedomScore(int dfg_node, int cell,
                             const std::vector<int>& freedom) const {
    const int available = freedom[cell];
    const int needed = degree_[dfg_node];
    if (needed > 3) return -available;
    if (available >= needed) return available;
    return 1000 + (needed - available);
  }

  int ChooseCPUMappingNearbyCell(int dfg_node, int anchor_cell,
                                 const PlacementState& state,
                                 const std::vector<int>& freedom) {
    int best_cell = -1;
    CellScore best_score;
    int equal_best_count = 0;
    for (int cell : compatible_cells_[dfg_node]) {
      if (!CanPlaceCPUMapping(dfg_node, cell, state)) continue;
      cell_visits_++;
      CellScore score;
      if (IsCPUMappingYOTO()) {
        score.value = {static_cast<double>(DistanceCost(anchor_cell, cell)),
                       static_cast<double>(OffsetOrder(anchor_cell, cell)),
                       0.0};
      } else {
        score.value = {static_cast<double>(DistanceCost(anchor_cell, cell)),
                       static_cast<double>(
                           CPUMappingFreedomScore(dfg_node, cell, freedom)),
                       static_cast<double>(OffsetOrder(anchor_cell, cell))};
      }
      if (best_cell < 0 || IsBetterScore(score, best_score)) {
        best_cell = cell;
        best_score = score;
        equal_best_count = 1;
      } else if (IsEqualScore(score, best_score)) {
        equal_best_count++;
        std::uniform_int_distribution<int> dist(0, equal_best_count - 1);
        if (dist(rng_) == 0) best_cell = cell;
      }
    }
    return best_cell;
  }

  void PlaceCPUMappingNode(int dfg_node, int cell, PlacementState& state,
                           std::vector<int>& freedom) const {
    PlaceNode(dfg_node, cell, state);
    UpdateFreedomGrid(cell, freedom);
  }

  bool PlaceCPUMappingInitialNode(int dfg_node, PlacementState& state,
                                  std::vector<int>& freedom) {
    if (state.dfg_to_cell[dfg_node] >= 0) return true;
    const int cell = ChooseCPUMappingInitialCell(dfg_node, state);
    if (cell < 0) return false;
    PlaceCPUMappingNode(dfg_node, cell, state, freedom);
    return true;
  }

  bool PlaceCPUMappingNearNode(int dfg_node, int anchor_node,
                               PlacementState& state,
                               std::vector<int>& freedom) {
    if (state.dfg_to_cell[dfg_node] >= 0) return true;
    const int anchor_cell = state.dfg_to_cell[anchor_node];
    if (anchor_cell < 0) return PlaceCPUMappingInitialNode(dfg_node, state, freedom);
    const int cell =
        ChooseCPUMappingNearbyCell(dfg_node, anchor_cell, state, freedom);
    if (cell >= 0) {
      PlaceCPUMappingNode(dfg_node, cell, state, freedom);
      return true;
    }
    return PlaceCPUMappingInitialNode(dfg_node, state, freedom);
  }

  std::optional<PlacementState> ConstructCPUMappingPlacement() {
    PlacementState state;
    state.dfg_to_cell.assign(dfg_.GetNodeNum(), -1);
    state.cell_to_dfg.assign(rows_ * cols_, -1);
    std::vector<int> freedom = InitialFreedomGrid();

    for (const auto& step : BuildCPUMappingPlan()) {
      const bool anchor_placed = state.dfg_to_cell[step.anchor] >= 0;
      const bool target_placed = state.dfg_to_cell[step.target] >= 0;
      if (!anchor_placed && !target_placed) {
        if (!PlaceCPUMappingInitialNode(step.anchor, state, freedom)) {
          return std::nullopt;
        }
        if (!PlaceCPUMappingNearNode(step.target, step.anchor, state, freedom)) {
          return std::nullopt;
        }
      } else if (anchor_placed && !target_placed) {
        if (!PlaceCPUMappingNearNode(step.target, step.anchor, state, freedom)) {
          return std::nullopt;
        }
      } else if (!anchor_placed && target_placed) {
        if (!PlaceCPUMappingNearNode(step.anchor, step.target, state, freedom)) {
          return std::nullopt;
        }
      }
    }

    for (int node = 0; node < dfg_.GetNodeNum(); node++) {
      if (!PlaceCPUMappingInitialNode(node, state, freedom)) {
        return std::nullopt;
      }
    }
    return state;
  }

  std::optional<PlacementState> RunCPUMappingMultiStart(
      std::chrono::steady_clock::time_point start) {
    std::optional<PlacementState> best;
    double best_cost = std::numeric_limits<double>::infinity();
    int trials = 0;
    for (int seed = 0; seed < SeedCount() && !HasTimedOut(start, 0.05); seed++) {
      ResetSeed(seed);
      for (int trial = 0; trial < MaxTrials() && !HasTimedOut(start, 0.05);
           trial++) {
        auto placement = ConstructCPUMappingPlacement();
        trials++;
        if (!placement.has_value()) continue;
        const double cost = PlacementCost(*placement);
        if (cost < best_cost) {
          best = placement;
          best_cost = cost;
        }
      }
    }
    Log("cpu_mapping_traversal trials=" + std::to_string(trials));
    return best;
  }

  int ChooseCellForStep(const Step& step, PlacementState& state) {
    if (state.dfg_to_cell[step.anchor] < 0) {
      const int anchor_cell = ChooseInitialCell(step.anchor, state);
      if (anchor_cell < 0) return -1;
      PlaceNode(step.anchor, anchor_cell, state);
    }
    const int anchor_cell = state.dfg_to_cell[step.anchor];
    return ChooseBestCell(step.target, state, [&](int cell) {
      if (UsesYOTOTraversalOrder()) {
        return CellScore{{static_cast<double>(DistanceCost(anchor_cell, cell)) +
                              ReservedIOSlotPenalty(step.target, cell),
                          IOScore(step.target, cell),
                          0.0}};
      }
      return CellScore{{IncrementalCost(step.target, cell, state) +
                            ReservedIOSlotPenalty(step.target, cell),
                        static_cast<double>(DistanceCost(anchor_cell, cell)),
                        IOScore(step.target, cell)}};
    });
  }

  std::optional<PlacementState> ConstructTraversalPlacement() {
    PlacementState state;
    state.dfg_to_cell.assign(dfg_.GetNodeNum(), -1);
    state.cell_to_dfg.assign(rows_ * cols_, -1);
    const auto plan = BuildTraversalPlan();
    for (const auto& step : plan) {
      if (state.dfg_to_cell[step.target] >= 0) continue;
      const int cell = ChooseCellForStep(step, state);
      if (cell < 0) return std::nullopt;
      PlaceNode(step.target, cell, state);
    }
    for (int node = 0; node < dfg_.GetNodeNum(); node++) {
      if (state.dfg_to_cell[node] >= 0) continue;
      const int cell = ChooseInitialCell(node, state);
      if (cell < 0) return std::nullopt;
      PlaceNode(node, cell, state);
    }
    return state;
  }

  std::optional<PlacementState> RunTraversalMultiStart(
      std::chrono::steady_clock::time_point start) {
    std::optional<PlacementState> best;
    double best_cost = std::numeric_limits<double>::infinity();
    int trials = 0;
    for (int seed = 0; seed < SeedCount() && !HasTimedOut(start, 0.05); seed++) {
      ResetSeed(seed);
      for (int trial = 0; trial < MaxTrials() && !HasTimedOut(start, 0.05);
           trial++) {
        auto placement = ConstructTraversalPlacement();
        trials++;
        if (!placement.has_value()) continue;
        const double cost = PlacementCost(*placement);
        if (cost < best_cost) {
          best = placement;
          best_cost = cost;
        }
      }
    }
    Log("array_traversal trials=" + std::to_string(trials));
    return best;
  }

  // PRISA initialization. SIS produces a low-bandwidth node order, then maps
  // labels onto the PRISA resource order before swap-based refinement starts.
  std::optional<PlacementState> RandomPlacement() {
    PlacementState state;
    state.dfg_to_cell.assign(dfg_.GetNodeNum(), -1);
    state.cell_to_dfg.assign(rows_ * cols_, -1);
    std::vector<int> nodes(dfg_.GetNodeNum());
    std::iota(nodes.begin(), nodes.end(), 0);
    std::shuffle(nodes.begin(), nodes.end(), rng_);
    std::stable_sort(nodes.begin(), nodes.end(), [&](int a, int b) {
      if (IsIONode(a) != IsIONode(b)) return IsIONode(a) > IsIONode(b);
      return degree_[a] > degree_[b];
    });
    for (int node : nodes) {
      auto cells = CompatibleCells(node, state);
      if (cells.empty()) return std::nullopt;
      std::uniform_int_distribution<int> dist(0, static_cast<int>(cells.size()) - 1);
      PlaceNode(node, cells[dist(rng_)], state);
    }
    return state;
  }

  int OtherEndpoint(int edge_id, int node) const {
    const auto& edge = edges_[edge_id];
    if (edge.source == node) return edge.target;
    if (edge.target == node) return edge.source;
    return -1;
  }

  std::vector<int> LowBandwidthNodeOrder() {
    const int n = dfg_.GetNodeNum();
    if (n == 0) return {};

    auto bfs_distance = [&](int root,
                            const std::vector<char>* allowed = nullptr) {
      std::vector<int> distance(n, -1);
      std::queue<int> q;
      distance[root] = 0;
      q.push(root);
      while (!q.empty()) {
        const int u = q.front();
        q.pop();
        for (int edge_id : incident_edge_ids_[u]) {
          const int v = OtherEndpoint(edge_id, u);
          if (v < 0 || distance[v] >= 0) continue;
          if (allowed != nullptr && !(*allowed)[v]) continue;
          distance[v] = distance[u] + 1;
          q.push(v);
        }
      }
      return distance;
    };

    std::vector<char> visited(n, 0);
    std::vector<std::vector<int>> components;
    for (int root = 0; root < n; root++) {
      if (visited[root]) continue;
      std::vector<int> component;
      std::queue<int> q;
      visited[root] = 1;
      q.push(root);
      while (!q.empty()) {
        const int u = q.front();
        q.pop();
        component.push_back(u);
        for (int edge_id : incident_edge_ids_[u]) {
          const int v = OtherEndpoint(edge_id, u);
          if (v < 0 || visited[v]) continue;
          visited[v] = 1;
          q.push(v);
        }
      }
      components.push_back(std::move(component));
    }

    std::sort(components.begin(), components.end(),
              [&](const std::vector<int>& a, const std::vector<int>& b) {
                if (a.size() != b.size()) return a.size() > b.size();
                const auto max_degree = [&](const std::vector<int>& component) {
                  int result = -1;
                  for (int node : component) {
                    result = std::max(result, degree_[node]);
                  }
                  return result;
                };
                const int a_degree = max_degree(a);
                const int b_degree = max_degree(b);
                if (a_degree != b_degree) return a_degree > b_degree;
                return *std::min_element(a.begin(), a.end()) <
                       *std::min_element(b.begin(), b.end());
              });

    std::vector<int> order;
    order.reserve(n);
    for (const auto& component : components) {
      std::vector<char> in_component(n, 0);
      for (int node : component) in_component[node] = 1;

      int highest_degree = component.front();
      for (int node : component) {
        if (degree_[node] > degree_[highest_degree] ||
            (degree_[node] == degree_[highest_degree] &&
             node < highest_degree)) {
          highest_degree = node;
        }
      }

      const auto distance_from_high_degree =
          bfs_distance(highest_degree, &in_component);
      int max_distance = 0;
      std::vector<int> farthest_nodes;
      for (int node : component) {
        if (distance_from_high_degree[node] < 0) continue;
        if (distance_from_high_degree[node] > max_distance) {
          max_distance = distance_from_high_degree[node];
          farthest_nodes.clear();
        }
        if (distance_from_high_degree[node] == max_distance) {
          farthest_nodes.push_back(node);
        }
      }
      if (farthest_nodes.empty()) farthest_nodes.push_back(highest_degree);
      const int start = *std::min_element(
          farthest_nodes.begin(), farthest_nodes.end(), [&](int a, int b) {
            if (degree_[a] != degree_[b]) return degree_[a] < degree_[b];
            return a < b;
          });

      std::vector<char> labeled(n, 0);
      std::queue<int> q;
      labeled[start] = 1;
      q.push(start);
      while (!q.empty()) {
        const int u = q.front();
        q.pop();
        order.push_back(u);

        std::vector<int> neighbors;
        for (int edge_id : incident_edge_ids_[u]) {
          const int v = OtherEndpoint(edge_id, u);
          if (v < 0 || !in_component[v] || labeled[v]) continue;
          labeled[v] = 1;
          neighbors.push_back(v);
        }
        std::sort(neighbors.begin(), neighbors.end(), [&](int a, int b) {
          if (degree_[a] != degree_[b]) return degree_[a] < degree_[b];
          return a < b;
        });
        for (int neighbor : neighbors) q.push(neighbor);
      }

      for (int node : component) {
        if (!labeled[node]) order.push_back(node);
      }
    }
    return order;
  }

  std::optional<PlacementState> ConstructPRISAInitialPlacementFromOrder(
      const std::vector<int>& node_order) {
    PlacementState state;
    state.dfg_to_cell.assign(dfg_.GetNodeNum(), -1);
    state.cell_to_dfg.assign(rows_ * cols_, -1);
    for (int label = 0; label < static_cast<int>(node_order.size());
         label++) {
      const int node = node_order[label];
      int best_cell = -1;
      int best_distance = std::numeric_limits<int>::max();
      for (int order_index = 0;
           order_index < static_cast<int>(prisa_cell_order_.size());
           order_index++) {
        const int cell = prisa_cell_order_[order_index];
        if (!CanPlace(node, cell, state)) continue;
        const int label_distance = std::abs(order_index - label);
        if (label_distance < best_distance) {
          best_distance = label_distance;
          best_cell = cell;
        }
      }
      if (best_cell < 0) return std::nullopt;
      PlaceNode(node, best_cell, state);
    }
    return state;
  }

  std::optional<PlacementState> ConstructPRISAInitialPlacement() {
    if (!UsesPRISASIS()) return RandomPlacement();
    return ConstructPRISAInitialPlacementFromOrder(LowBandwidthNodeOrder());
  }

  // PRISA weak/potential region accounting and local swap deltas. These helpers
  // avoid recomputing all edges after every candidate swap.
  bool PRISAIsWeak(int row, int column) const {
    const int resource_count = static_cast<int>(prisa_cell_order_.size());
    if (row == column || row < 0 || column < 0 || row >= resource_count ||
        column >= resource_count) {
      return false;
    }
    return prisa_weak_region_matrix_[row * resource_count + column] != 0;
  }

  bool PRISAIsPotential(int row, int column) const {
    return row != column && !PRISAIsWeak(row, column);
  }

  bool PRISAEdgeRegion(const PlacementState& state, int edge_id,
                       bool* is_weak, bool* is_potential) const {
    const auto& edge = edges_[edge_id];
    const int source_cell = state.dfg_to_cell[edge.source];
    const int target_cell = state.dfg_to_cell[edge.target];
    if (source_cell < 0 || target_cell < 0) return false;
    const int row = prisa_order_index_[source_cell];
    const int column = prisa_order_index_[target_cell];
    if (row < 0 || column < 0 || row == column) return false;
    *is_weak = PRISAIsWeak(row, column);
    *is_potential = !*is_weak;
    return true;
  }

  void AddPRISAEdgeToStats(const PlacementState& state, int edge_id,
                           PRISAStats& stats, int sign) const {
    const auto& edge = edges_[edge_id];
    const int source_cell = state.dfg_to_cell[edge.source];
    const int target_cell = state.dfg_to_cell[edge.target];
    if (source_cell < 0 || target_cell < 0) return;
    const int row = prisa_order_index_[source_cell];
    const int column = prisa_order_index_[target_cell];
    if (row < 0 || column < 0 || row == column) return;
    bool is_weak = false;
    bool is_potential = false;
    if (!PRISAEdgeRegion(state, edge_id, &is_weak, &is_potential)) return;
    if (is_weak) {
      stats.row_weak[row] += sign;
      stats.column_weak[column] += sign;
      stats.weak_edges += sign;
      stats.edge_weak[edge_id] = sign > 0;
      stats.edge_potential[edge_id] = 0;
    } else if (is_potential) {
      stats.row_potential[row] += sign;
      stats.column_potential[column] += sign;
      stats.edge_potential[edge_id] = sign > 0;
      stats.edge_weak[edge_id] = 0;
    }
  }

  PRISAStats InitializePRISAStats(const PlacementState& state) const {
    const int resource_count = static_cast<int>(prisa_cell_order_.size());
    PRISAStats stats;
    stats.row_weak.assign(resource_count, 0);
    stats.column_weak.assign(resource_count, 0);
    stats.row_potential.assign(resource_count, 0);
    stats.column_potential.assign(resource_count, 0);
    stats.edge_weak.assign(edges_.size(), 0);
    stats.edge_potential.assign(edges_.size(), 0);
    stats.weak_edges = 0;
    for (int edge_id = 0; edge_id < static_cast<int>(edges_.size());
         edge_id++) {
      AddPRISAEdgeToStats(state, edge_id, stats, 1);
    }
    return stats;
  }

  std::vector<int> AffectedEdgesForCellPair(const PlacementState& state,
                                            int cell_a, int cell_b) const {
    std::vector<int> result;
    const int node_a = state.cell_to_dfg[cell_a];
    const int node_b = state.cell_to_dfg[cell_b];
    if (node_a >= 0) {
      result.insert(result.end(), incident_edge_ids_[node_a].begin(),
                    incident_edge_ids_[node_a].end());
    }
    if (node_b >= 0) {
      result.insert(result.end(), incident_edge_ids_[node_b].begin(),
                    incident_edge_ids_[node_b].end());
    }
    std::sort(result.begin(), result.end());
    result.erase(std::unique(result.begin(), result.end()), result.end());
    return result;
  }

  int CellAfterSwap(int cell, int first_cell, int second_cell) const {
    if (cell == first_cell) return second_cell;
    if (cell == second_cell) return first_cell;
    return cell;
  }

  int EdgeWeakAfterSwap(const PlacementState& state, int edge_id,
                        int first_cell, int second_cell) const {
    const auto& edge = edges_[edge_id];
    const int source_cell =
        CellAfterSwap(state.dfg_to_cell[edge.source], first_cell, second_cell);
    const int target_cell =
        CellAfterSwap(state.dfg_to_cell[edge.target], first_cell, second_cell);
    const int row = prisa_order_index_[source_cell];
    const int column = prisa_order_index_[target_cell];
    return PRISAIsWeak(row, column) ? 1 : 0;
  }

  int PRISAWeakDeltaForCellSwap(
      const PlacementState& state, int first_cell, int second_cell,
      const std::vector<int>& affected_edges) const {
    int before = 0;
    int after = 0;
    for (int edge_id : affected_edges) {
      bool is_weak = false;
      bool is_potential = false;
      if (PRISAEdgeRegion(state, edge_id, &is_weak, &is_potential) &&
          is_weak) {
        before++;
      }
      after += EdgeWeakAfterSwap(state, edge_id, first_cell, second_cell);
    }
    return after - before;
  }

  double PlacementCostDeltaForCellSwap(
      const PlacementState& state, int first_cell, int second_cell,
      const std::vector<int>& affected_edges) const {
    double before = 0.0;
    double after = 0.0;
    for (int edge_id : affected_edges) {
      const auto& edge = edges_[edge_id];
      const int source_cell = state.dfg_to_cell[edge.source];
      const int target_cell = state.dfg_to_cell[edge.target];
      before += DistanceCost(source_cell, target_cell);
      after += DistanceCost(CellAfterSwap(source_cell, first_cell, second_cell),
                            CellAfterSwap(target_cell, first_cell, second_cell));
    }
    return after - before;
  }

  int DirectEdgeDeltaForCellSwap(
      const PlacementState& state, int first_cell, int second_cell,
      const std::vector<int>& affected_edges) const {
    int before = 0;
    int after = 0;
    for (int edge_id : affected_edges) {
      const auto& edge = edges_[edge_id];
      const int source_cell = state.dfg_to_cell[edge.source];
      const int target_cell = state.dfg_to_cell[edge.target];
      before += DistanceCost(source_cell, target_cell) <= 1 ? 1 : 0;
      after += DistanceCost(CellAfterSwap(source_cell, first_cell, second_cell),
                            CellAfterSwap(target_cell, first_cell,
                                          second_cell)) <= 1
                   ? 1
                   : 0;
    }
    return after - before;
  }

  int AffectedMaxDistanceAfterCellSwap(
      const PlacementState& state, int first_cell, int second_cell,
      const std::vector<int>& affected_edges) const {
    int result = 0;
    for (int edge_id : affected_edges) {
      const auto& edge = edges_[edge_id];
      const int source_cell = state.dfg_to_cell[edge.source];
      const int target_cell = state.dfg_to_cell[edge.target];
      result = std::max(
          result, DistanceCost(CellAfterSwap(source_cell, first_cell,
                                             second_cell),
                               CellAfterSwap(target_cell, first_cell,
                                             second_cell)));
    }
    return result;
  }

  bool CanSwapCells(const PlacementState& state, int first_cell,
                    int second_cell) const {
    if (first_cell == second_cell || first_cell < 0 || second_cell < 0) {
      return false;
    }
    const int first_node = state.cell_to_dfg[first_cell];
    const int second_node = state.cell_to_dfg[second_cell];
    if (first_node < 0 && second_node < 0) return false;
    if (first_node >= 0 && !IsCompatible(first_node, second_cell)) {
      return false;
    }
    if (second_node >= 0 && !IsCompatible(second_node, first_cell)) {
      return false;
    }
    return true;
  }

  bool ApplyCellSwapWithStats(PlacementState& state, PRISAStats& stats,
                              int first_cell, int second_cell) const {
    if (!CanSwapCells(state, first_cell, second_cell)) return false;
    const auto affected_edges =
        AffectedEdgesForCellPair(state, first_cell, second_cell);
    for (int edge_id : affected_edges) {
      AddPRISAEdgeToStats(state, edge_id, stats, -1);
    }

    const int first_node = state.cell_to_dfg[first_cell];
    const int second_node = state.cell_to_dfg[second_cell];
    state.cell_to_dfg[first_cell] = second_node;
    state.cell_to_dfg[second_cell] = first_node;
    if (first_node >= 0) state.dfg_to_cell[first_node] = second_cell;
    if (second_node >= 0) state.dfg_to_cell[second_node] = first_cell;

    for (int edge_id : affected_edges) {
      AddPRISAEdgeToStats(state, edge_id, stats, 1);
    }
    return true;
  }

  bool ApplyCellSwap(PlacementState& state, int first_cell,
                     int second_cell) const {
    if (!CanSwapCells(state, first_cell, second_cell)) return false;
    const int first_node = state.cell_to_dfg[first_cell];
    const int second_node = state.cell_to_dfg[second_cell];
    state.cell_to_dfg[first_cell] = second_node;
    state.cell_to_dfg[second_cell] = first_node;
    if (first_node >= 0) state.dfg_to_cell[first_node] = second_cell;
    if (second_node >= 0) state.dfg_to_cell[second_node] = first_cell;
    return true;
  }

  // PRISA move generation. Paper-faithful variants prioritize weak-region
  // repairs; cost-aware variants additionally sample placement-quality moves.
  struct PRISAProposal {
    int first_cell = -1;
    int second_cell = -1;
    double cost_delta = 0.0;
    int direct_delta = 0;
    int affected_max_after = 0;
  };

  std::optional<PRISAProposal> ProposeRandomPRISASwap(
      const PlacementState& state) {
    if (prisa_cell_order_.size() < 2) return std::nullopt;
    std::uniform_int_distribution<int> dist(
        0, static_cast<int>(prisa_cell_order_.size()) - 1);
    for (int attempt = 0; attempt < 32; attempt++) {
      const int first_cell = prisa_cell_order_[dist(rng_)];
      const int second_cell = prisa_cell_order_[dist(rng_)];
      if (!CanSwapCells(state, first_cell, second_cell)) continue;
      const auto affected_edges =
          AffectedEdgesForCellPair(state, first_cell, second_cell);
      return PRISAProposal{
          first_cell, second_cell,
          PlacementCostDeltaForCellSwap(state, first_cell, second_cell,
                                        affected_edges),
          DirectEdgeDeltaForCellSwap(state, first_cell, second_cell,
                                     affected_edges),
          AffectedMaxDistanceAfterCellSwap(state, first_cell, second_cell,
                                           affected_edges)};
    }
    return std::nullopt;
  }

  std::optional<PRISAProposal> ProposePlacementCostSampledSwap(
      const PlacementState& state, int requested_sample_count = -1) {
    const int resource_count = static_cast<int>(prisa_cell_order_.size());
    if (resource_count < 2) return std::nullopt;
    const int total_pairs = resource_count * (resource_count - 1) / 2;
    const int default_sample_count = std::max(16, resource_count * 4);
    const int sample_count = std::min(
        total_pairs, requested_sample_count > 0 ? requested_sample_count
                                                : default_sample_count);
    std::optional<PRISAProposal> best;

    auto is_better_fallback = [&](const PRISAProposal& candidate,
                                  const PRISAProposal& current_best) {
      if (candidate.cost_delta + 1.0e-9 < current_best.cost_delta) {
        return true;
      }
      if (candidate.cost_delta > current_best.cost_delta + 1.0e-9) {
        return false;
      }
      if (UsesCostAwarePRISA()) {
        if (candidate.direct_delta != current_best.direct_delta) {
          return candidate.direct_delta > current_best.direct_delta;
        }
        if (candidate.affected_max_after != current_best.affected_max_after) {
          return candidate.affected_max_after < current_best.affected_max_after;
        }
      }
      return false;
    };

    auto consider_pair = [&](int first_index, int second_index) {
      const int first_cell = prisa_cell_order_[first_index];
      const int second_cell = prisa_cell_order_[second_index];
      if (!CanSwapCells(state, first_cell, second_cell)) return;
      const auto affected_edges =
          AffectedEdgesForCellPair(state, first_cell, second_cell);
      const double cost_delta =
          PlacementCostDeltaForCellSwap(state, first_cell, second_cell,
                                        affected_edges);
      PRISAProposal candidate{
          first_cell,
          second_cell,
          cost_delta,
          DirectEdgeDeltaForCellSwap(state, first_cell, second_cell,
                                     affected_edges),
          AffectedMaxDistanceAfterCellSwap(state, first_cell, second_cell,
                                           affected_edges)};
      if (!best.has_value() || is_better_fallback(candidate, *best)) {
        best = candidate;
      }
    };

    if (sample_count == total_pairs) {
      for (int first = 0; first < resource_count; first++) {
        for (int second = first + 1; second < resource_count; second++) {
          consider_pair(first, second);
        }
      }
    } else {
      std::uniform_int_distribution<int> dist(0, resource_count - 1);
      for (int attempt = 0; attempt < sample_count * 4; attempt++) {
        const int first = dist(rng_);
        const int second = dist(rng_);
        if (first == second) continue;
        consider_pair(std::min(first, second), std::max(first, second));
      }
    }
    if (best.has_value()) return best;
    return ProposeRandomPRISASwap(state);
  }

  int PRISALightQualitySampleCount() const {
    const int resource_count = static_cast<int>(prisa_cell_order_.size());
    // SIS already gives PRISA a low-bandwidth starting point.  Keep the
    // post-WR quality refinement cheaper than no-SIS so the runtime trend
    // matches the paper without using the cost-aware derived mapper.
    if (UsesPRISASIS()) return std::max(8, resource_count / 3);
    return std::max(16, resource_count);
  }

  double CostAwarePRISALocalSwapScore(
      const PlacementState& state, int first_cell, int second_cell,
      const std::vector<int>& affected_edges) const {
    double mesh_delta = 0.0;
    int after_max_hop = 0;
    int direct_edge_gain = 0;
    for (int edge_id : affected_edges) {
      const auto& edge = edges_[edge_id];
      const int source_cell = state.dfg_to_cell[edge.source];
      const int target_cell = state.dfg_to_cell[edge.target];
      const int before = DistanceCost(source_cell, target_cell);
      const int after =
          DistanceCost(CellAfterSwap(source_cell, first_cell, second_cell),
                       CellAfterSwap(target_cell, first_cell, second_cell));
      mesh_delta += after - before;
      after_max_hop = std::max(after_max_hop, after);
      if (before <= 1 && after > 1) direct_edge_gain--;
      if (before > 1 && after <= 1) direct_edge_gain++;
    }

    const int weak_delta =
        PRISAWeakDeltaForCellSwap(state, first_cell, second_cell,
                                  affected_edges);
    return weak_delta * 1000000.0 + mesh_delta * 100.0 +
           after_max_hop * 20.0 - direct_edge_gain * 80.0;
  }

  int CostAwarePRISACandidateSampleCount() const {
    if (max_trials_.has_value()) return std::max(16, max_trials_.value());
    return 512;
  }

  int CostAwarePRISAFullEvaluationCount() const { return 8; }

  std::optional<PRISAProposal> ProposeCostAwarePRISASwap(
      const PlacementState& state) {
    const int resource_count = static_cast<int>(prisa_cell_order_.size());
    if (resource_count < 2) return std::nullopt;

    const int total_pairs = resource_count * (resource_count - 1) / 2;
    const int sample_count =
        std::min(total_pairs, CostAwarePRISACandidateSampleCount());
    auto normalize_pair = [](int lhs, int rhs) {
      if (lhs > rhs) std::swap(lhs, rhs);
      return std::pair<int, int>(lhs, rhs);
    };

    std::set<std::pair<int, int>> sampled_pairs;
    if (sample_count == total_pairs) {
      for (int first = 0; first < resource_count; first++) {
        for (int second = first + 1; second < resource_count; second++) {
          sampled_pairs.insert({first, second});
        }
      }
    } else {
      std::vector<std::pair<int, int>> edge_hops;
      edge_hops.reserve(edges_.size());
      for (int edge_id = 0; edge_id < static_cast<int>(edges_.size());
           edge_id++) {
        const auto& edge = edges_[edge_id];
        const int source_cell = state.dfg_to_cell[edge.source];
        const int target_cell = state.dfg_to_cell[edge.target];
        if (source_cell < 0 || target_cell < 0) continue;
        edge_hops.push_back({DistanceCost(source_cell, target_cell), edge_id});
      }
      std::sort(edge_hops.begin(), edge_hops.end(),
                std::greater<std::pair<int, int>>());

      const int focused_budget = std::max(1, sample_count / 2);
      const int focused_edge_count =
          std::min(4, static_cast<int>(edge_hops.size()));
      for (int index = 0;
           index < focused_edge_count &&
           static_cast<int>(sampled_pairs.size()) < focused_budget;
           index++) {
        const auto& edge = edges_[edge_hops[index].second];
        const int endpoint_cells[] = {state.dfg_to_cell[edge.source],
                                      state.dfg_to_cell[edge.target]};
        for (int endpoint_cell : endpoint_cells) {
          const int endpoint_index = prisa_order_index_[endpoint_cell];
          if (endpoint_index < 0) continue;
          for (int other = 0;
               other < resource_count &&
               static_cast<int>(sampled_pairs.size()) < focused_budget;
               other++) {
            if (other == endpoint_index) continue;
            sampled_pairs.insert(normalize_pair(endpoint_index, other));
          }
        }
      }

      std::uniform_int_distribution<int> dist(0, resource_count - 1);
      int attempts = 0;
      while (static_cast<int>(sampled_pairs.size()) < sample_count &&
             attempts < sample_count * 16) {
        attempts++;
        const int first = dist(rng_);
        const int second = dist(rng_);
        if (first == second) continue;
        sampled_pairs.insert(normalize_pair(first, second));
      }
    }

    struct Candidate {
      double score = std::numeric_limits<double>::infinity();
      int first_cell = -1;
      int second_cell = -1;
      int random_order = 0;
    };

    std::vector<Candidate> candidates;
    candidates.reserve(sampled_pairs.size());
    int random_order = 0;
    for (const auto& [first_index, second_index] : sampled_pairs) {
      const int first_cell = prisa_cell_order_[first_index];
      const int second_cell = prisa_cell_order_[second_index];
      if (!CanSwapCells(state, first_cell, second_cell)) continue;
      const auto affected_edges =
          AffectedEdgesForCellPair(state, first_cell, second_cell);
      candidates.push_back(
          {CostAwarePRISALocalSwapScore(state, first_cell, second_cell,
                                        affected_edges),
           first_cell, second_cell, random_order++});
    }
    if (candidates.empty()) return ProposeRandomPRISASwap(state);

    std::shuffle(candidates.begin(), candidates.end(), rng_);
    std::sort(candidates.begin(), candidates.end(),
              [](const Candidate& a, const Candidate& b) {
                if (a.score != b.score) return a.score < b.score;
                return a.random_order < b.random_order;
              });

    const int full_evaluation_count =
        std::min(CostAwarePRISAFullEvaluationCount(),
                 static_cast<int>(candidates.size()));
    const double current_score = CostAwarePRISAScore(state);
    for (int i = 0; i < full_evaluation_count; i++) {
      PlacementState next = state;
      if (!ApplyCellSwap(next, candidates[i].first_cell,
                         candidates[i].second_cell)) {
        candidates[i].score = kImpossibleCost;
        continue;
      }
      candidates[i].score = CostAwarePRISAScore(next);
    }
    candidates.resize(full_evaluation_count);
    std::sort(candidates.begin(), candidates.end(),
              [](const Candidate& a, const Candidate& b) {
                if (a.score != b.score) return a.score < b.score;
                return a.random_order < b.random_order;
              });

    int selection_count = 1;
    if (candidates.front().score >= current_score) {
      selection_count = std::min(4, static_cast<int>(candidates.size()));
    } else {
      while (selection_count < static_cast<int>(candidates.size()) &&
             candidates[selection_count].score == candidates.front().score) {
        selection_count++;
      }
    }

    std::uniform_int_distribution<int> dist(0, selection_count - 1);
    const Candidate& chosen = candidates[dist(rng_)];
    return PRISAProposal{chosen.first_cell, chosen.second_cell,
                         chosen.score - current_score, 0, 0};
  }

  std::vector<int> PRISAWeakCounterparts(const PlacementState& state,
                                         const PRISAStats& stats,
                                         int first_index,
                                         bool row_side) const {
    std::vector<int> counterparts;
    counterparts.reserve(row_side ? stats.row_weak[first_index]
                                  : stats.column_weak[first_index]);
    for (int edge_id = 0; edge_id < static_cast<int>(edges_.size());
         edge_id++) {
      if (!stats.edge_weak[edge_id]) continue;
      const auto& edge = edges_[edge_id];
      const int source_cell = state.dfg_to_cell[edge.source];
      const int target_cell = state.dfg_to_cell[edge.target];
      const int row = prisa_order_index_[source_cell];
      const int column = prisa_order_index_[target_cell];
      if (row_side) {
        if (row == first_index) counterparts.push_back(column);
      } else {
        if (column == first_index) counterparts.push_back(row);
      }
    }
    return counterparts;
  }

  struct PRISACandidate {
    int first_index = -1;
    int second_index = -1;
    int fixed_weak_count = 0;
    int potential_count = 0;
    int weak_delta = 0;
    double cost_delta = 0.0;
    int direct_delta = 0;
    int affected_max_after = 0;
    int spatial_distance = 0;
    int random_order = 0;
  };

  bool IsBetterPRISACandidate(const PRISACandidate& candidate,
                              const PRISACandidate& best) const {
    if (UsesCostAwarePRISA()) {
      if (candidate.potential_count != best.potential_count) {
        return candidate.potential_count < best.potential_count;
      }
      if (candidate.fixed_weak_count != best.fixed_weak_count) {
        return candidate.fixed_weak_count > best.fixed_weak_count;
      }
      if (candidate.weak_delta != best.weak_delta) {
        return candidate.weak_delta < best.weak_delta;
      }
      if (candidate.cost_delta != best.cost_delta) {
        return candidate.cost_delta < best.cost_delta;
      }
      if (candidate.affected_max_after != best.affected_max_after) {
        return candidate.affected_max_after < best.affected_max_after;
      }
      if (candidate.direct_delta != best.direct_delta) {
        return candidate.direct_delta > best.direct_delta;
      }
      return candidate.random_order < best.random_order;
    }
    if (candidate.potential_count != best.potential_count) {
      return candidate.potential_count < best.potential_count;
    }
    if (candidate.fixed_weak_count != best.fixed_weak_count) {
      return candidate.fixed_weak_count > best.fixed_weak_count;
    }
    if (candidate.weak_delta != best.weak_delta) {
      return candidate.weak_delta < best.weak_delta;
    }
    if (candidate.cost_delta != best.cost_delta) {
      return candidate.cost_delta < best.cost_delta;
    }
    if (candidate.direct_delta != best.direct_delta) {
      return candidate.direct_delta > best.direct_delta;
    }
    if (candidate.affected_max_after != best.affected_max_after) {
      return candidate.affected_max_after < best.affected_max_after;
    }
    if (candidate.spatial_distance != best.spatial_distance) {
      return candidate.spatial_distance < best.spatial_distance;
    }
    return candidate.random_order < best.random_order;
  }

  std::optional<PRISACandidate> BestPRISACandidateFor(
      const PlacementState& state, const PRISAStats& stats, int first_index,
      bool row_side) {
    const auto weak_counterparts =
        PRISAWeakCounterparts(state, stats, first_index, row_side);
    if (weak_counterparts.empty()) return std::nullopt;

    const int resource_count = static_cast<int>(prisa_cell_order_.size());
    const int first_cell = prisa_cell_order_[first_index];
    std::uniform_int_distribution<int> start_dist(0, resource_count - 1);
    const int start = start_dist(rng_);

    struct CandidateKey {
      int second_index = -1;
      int fixed_weak_count = 0;
      int potential_count = 0;
      int spatial_distance = 0;
      int random_order = 0;
    };

    std::vector<CandidateKey> filtered;
    int best_potential_count = std::numeric_limits<int>::max();
    int best_fixed_weak_count = std::numeric_limits<int>::min();

    for (int offset = 0; offset < resource_count; offset++) {
      const int second_index = (start + offset) % resource_count;
      if (second_index == first_index) continue;
      const int second_cell = prisa_cell_order_[second_index];
      if (!CanSwapCells(state, first_cell, second_cell)) continue;

      int fixed_weak_count = 0;
      for (int counterpart : weak_counterparts) {
        const bool moves_to_pr =
            row_side ? PRISAIsPotential(second_index, counterpart)
                     : PRISAIsPotential(counterpart, second_index);
        if (moves_to_pr) fixed_weak_count++;
      }
      if (fixed_weak_count == 0) continue;

      const int potential_count =
          row_side ? stats.row_potential[second_index]
                   : stats.column_potential[second_index];
      if (potential_count < best_potential_count ||
          (potential_count == best_potential_count &&
           fixed_weak_count > best_fixed_weak_count)) {
        filtered.clear();
        best_potential_count = potential_count;
        best_fixed_weak_count = fixed_weak_count;
      }
      if (potential_count == best_potential_count &&
          fixed_weak_count == best_fixed_weak_count) {
        filtered.push_back(
            {second_index, fixed_weak_count, potential_count,
             DistanceCost(first_cell, second_cell), offset});
      }
    }

    std::optional<PRISACandidate> best;
    for (const auto& key : filtered) {
      const int second_cell = prisa_cell_order_[key.second_index];
      const auto affected_edges =
          AffectedEdgesForCellPair(state, first_cell, second_cell);
      PRISACandidate candidate{
          first_index,
          key.second_index,
          key.fixed_weak_count,
          key.potential_count,
          PRISAWeakDeltaForCellSwap(state, first_cell, second_cell,
                                    affected_edges),
          PlacementCostDeltaForCellSwap(state, first_cell, second_cell,
                                        affected_edges),
          DirectEdgeDeltaForCellSwap(state, first_cell, second_cell,
                                     affected_edges),
          AffectedMaxDistanceAfterCellSwap(state, first_cell, second_cell,
                                           affected_edges),
          key.spatial_distance,
          key.random_order};
      if (!best.has_value() || IsBetterPRISACandidate(candidate, *best)) {
        best = candidate;
      }
    }
    return best;
  }

  int RandomArgmax(const std::vector<int>& values) {
    int best = std::numeric_limits<int>::min();
    int best_index = -1;
    int equal_count = 0;
    for (int i = 0; i < static_cast<int>(values.size()); i++) {
      if (values[i] > best) {
        best = values[i];
        best_index = i;
        equal_count = 1;
      } else if (values[i] == best) {
        equal_count++;
        std::uniform_int_distribution<int> dist(0, equal_count - 1);
        if (dist(rng_) == 0) best_index = i;
      }
    }
    return best_index;
  }

  std::optional<PRISAProposal> ProposePRISAMove(const PlacementState& state,
                                                const PRISAStats& stats) {
    if (UsesCostAwarePRISA()) return ProposeCostAwarePRISASwap(state);
    if (stats.weak_edges <= 0) {
      // After all weak-region edges are repaired, PRISA should not fall into
      // the expensive cost-aware search used by the derived mapper.  A small
      // random sample keeps the paper-style random-swap refinement cheap while
      // still allowing annealing to improve placement quality.
      return ProposePlacementCostSampledSwap(state,
                                             PRISALightQualitySampleCount());
    }

    const int row_index = RandomArgmax(stats.row_weak);
    const int column_index = RandomArgmax(stats.column_weak);
    if (row_index < 0 || column_index < 0) {
      return ProposePlacementCostSampledSwap(state,
                                             PRISALightQualitySampleCount());
    }
    const int row_max = stats.row_weak[row_index];
    const int column_max = stats.column_weak[column_index];
    if (row_max <= 0 && column_max <= 0) {
      return ProposePlacementCostSampledSwap(state,
                                             PRISALightQualitySampleCount());
    }

    std::optional<PRISACandidate> candidate;
    const bool prefer_row =
        row_max > column_max ||
        (row_max == column_max &&
         std::uniform_int_distribution<int>(0, 1)(rng_) == 0);
    if (prefer_row) {
      candidate = BestPRISACandidateFor(state, stats, row_index, true);
      if (!candidate.has_value()) {
        candidate = BestPRISACandidateFor(state, stats, column_index, false);
      }
    } else {
      candidate = BestPRISACandidateFor(state, stats, column_index, false);
      if (!candidate.has_value()) {
        candidate = BestPRISACandidateFor(state, stats, row_index, true);
      }
    }
    if (!candidate.has_value()) {
      return ProposePlacementCostSampledSwap(state,
                                             PRISALightQualitySampleCount());
    }
    return PRISAProposal{prisa_cell_order_[candidate->first_index],
                         prisa_cell_order_[candidate->second_index],
                         candidate->cost_delta,
                         candidate->direct_delta,
                         candidate->affected_max_after};
  }

  // PRISA annealing and optional cost-aware polish.
  int PRISAMaxIterations() const {
    if (max_iterations_.has_value()) return std::max(1, max_iterations_.value());
    return std::max(1, dfg_.GetNodeNum() * 10);
  }

  int PRISAPolishPasses() const {
    if (!UsesCostAwarePRISA()) return 0;
    return std::max(4, std::min(16, PRISAMaxIterations() / 80));
  }

  PlacementState PolishPRISAQuality(
      PlacementState state, std::chrono::steady_clock::time_point start) {
    PlacementQuality current = ComputePlacementQuality(state);
    const int resource_count = static_cast<int>(prisa_cell_order_.size());
    if (resource_count < 2) return state;

    for (int pass = 0; pass < PRISAPolishPasses() && !HasTimedOut(start, 0.05);
         pass++) {
      bool improved = false;
      PlacementState best_state = state;
      PlacementQuality best_quality = current;

      for (int first_index = 0; first_index < resource_count; first_index++) {
        for (int second_index = first_index + 1; second_index < resource_count;
             second_index++) {
          const int first_cell = prisa_cell_order_[first_index];
          const int second_cell = prisa_cell_order_[second_index];
          if (!CanSwapCells(state, first_cell, second_cell)) continue;
          const auto affected_edges =
              AffectedEdgesForCellPair(state, first_cell, second_cell);
          const int direct_delta = DirectEdgeDeltaForCellSwap(
              state, first_cell, second_cell, affected_edges);
          const double cost_delta = PlacementCostDeltaForCellSwap(
              state, first_cell, second_cell, affected_edges);
          if (direct_delta < 0) continue;
          if (cost_delta > 1.0e-9) continue;
          if (direct_delta == 0 && cost_delta >= -1.0e-9) continue;

          PlacementState next = state;
          if (!ApplyCellSwap(next, first_cell, second_cell)) continue;
          const PlacementQuality next_quality = ComputePlacementQuality(next);
          if (IsBetterQuality(next_quality, best_quality)) {
            best_quality = next_quality;
            best_state = std::move(next);
            improved = true;
          }
        }
      }

      if (!improved) break;
      state = std::move(best_state);
      current = best_quality;
    }
    return state;
  }

  int DirectEdgeGainForCellSwap(const PlacementState& state, int first_cell,
                                int second_cell,
                                const std::vector<int>& affected_edges) const {
    int gain = 0;
    for (int edge_id : affected_edges) {
      const auto& edge = edges_[edge_id];
      const int source_cell = state.dfg_to_cell[edge.source];
      const int target_cell = state.dfg_to_cell[edge.target];
      const int before = DistanceCost(source_cell, target_cell);
      const int after =
          DistanceCost(CellAfterSwap(source_cell, first_cell, second_cell),
                       CellAfterSwap(target_cell, first_cell, second_cell));
      if (before <= 1 && after > 1) gain--;
      if (before > 1 && after <= 1) gain++;
    }
    return gain;
  }

  PlacementState PolishCostAwarePRISADirectEdges(
      PlacementState state, std::chrono::steady_clock::time_point start) {
    const int resource_count = static_cast<int>(prisa_cell_order_.size());
    if (resource_count < 2) return state;

    struct Candidate {
      int direct_gain = 0;
      double local_score = std::numeric_limits<double>::infinity();
      int first_cell = -1;
      int second_cell = -1;
    };

    CostAwarePRISAMetrics current = ComputeCostAwarePRISAMetrics(state);
    for (int pass = 0; pass < PRISAPolishPasses() && !HasTimedOut(start, 0.05);
         pass++) {
      std::vector<Candidate> candidates;
      candidates.reserve(resource_count * resource_count / 2);
      for (int first_index = 0; first_index < resource_count; first_index++) {
        for (int second_index = first_index + 1; second_index < resource_count;
             second_index++) {
          const int first_cell = prisa_cell_order_[first_index];
          const int second_cell = prisa_cell_order_[second_index];
          if (!CanSwapCells(state, first_cell, second_cell)) continue;
          const auto affected_edges =
              AffectedEdgesForCellPair(state, first_cell, second_cell);
          const int direct_gain =
              DirectEdgeGainForCellSwap(state, first_cell, second_cell,
                                        affected_edges);
          if (direct_gain <= 0) continue;
          candidates.push_back(
              {direct_gain,
               CostAwarePRISALocalSwapScore(state, first_cell, second_cell,
                                            affected_edges),
               first_cell, second_cell});
        }
      }
      if (candidates.empty()) break;

      const int reevaluate_count =
          std::min(256, static_cast<int>(candidates.size()));
      if (reevaluate_count < static_cast<int>(candidates.size())) {
        std::nth_element(
            candidates.begin(), candidates.begin() + reevaluate_count,
            candidates.end(), [](const Candidate& a, const Candidate& b) {
              if (a.direct_gain != b.direct_gain) {
                return a.direct_gain > b.direct_gain;
              }
              return a.local_score < b.local_score;
            });
      }
      candidates.resize(reevaluate_count);

      bool improved = false;
      PlacementState best_state = state;
      CostAwarePRISAMetrics best_metrics = current;
      for (const auto& candidate : candidates) {
        PlacementState next = state;
        if (!ApplyCellSwap(next, candidate.first_cell, candidate.second_cell)) {
          continue;
        }
        const CostAwarePRISAMetrics next_metrics =
            ComputeCostAwarePRISAMetrics(next);
        if (next_metrics.direct_edge_count <= best_metrics.direct_edge_count) {
          continue;
        }
        if (next_metrics.max_mesh_hop > current.max_mesh_hop) continue;
        if (next_metrics.mapped_lp_mesh_hop > current.mapped_lp_mesh_hop) {
          continue;
        }
        if (next_metrics.mesh_hop_sum > current.mesh_hop_sum + 2.0 + 1.0e-9) {
          continue;
        }
        if (next_metrics.direct_edge_count > best_metrics.direct_edge_count ||
            (next_metrics.direct_edge_count == best_metrics.direct_edge_count &&
             next_metrics.mesh_hop_sum < best_metrics.mesh_hop_sum)) {
          best_state = std::move(next);
          best_metrics = next_metrics;
          improved = true;
        }
      }
      if (!improved) break;
      state = std::move(best_state);
      current = best_metrics;
    }
    return state;
  }

  bool IsCostAwareParetoImprovement(
      const CostAwarePRISAMetrics& candidate,
      const CostAwarePRISAMetrics& current) const {
    if (candidate.mesh_hop_sum > current.mesh_hop_sum + 1.0e-9) return false;
    if (candidate.max_mesh_hop > current.max_mesh_hop) return false;
    if (candidate.mapped_lp_mesh_hop > current.mapped_lp_mesh_hop) {
      return false;
    }
    return candidate.direct_edge_count > current.direct_edge_count ||
           candidate.mesh_hop_sum + 1.0e-9 < current.mesh_hop_sum ||
           candidate.max_mesh_hop < current.max_mesh_hop ||
           candidate.mapped_lp_mesh_hop < current.mapped_lp_mesh_hop;
  }

  bool IsBetterCostAwareParetoCandidate(
      const CostAwarePRISAMetrics& candidate,
      const CostAwarePRISAMetrics& best) const {
    if (candidate.direct_edge_count != best.direct_edge_count) {
      return candidate.direct_edge_count > best.direct_edge_count;
    }
    if (candidate.max_mesh_hop != best.max_mesh_hop) {
      return candidate.max_mesh_hop < best.max_mesh_hop;
    }
    if (candidate.mapped_lp_mesh_hop != best.mapped_lp_mesh_hop) {
      return candidate.mapped_lp_mesh_hop < best.mapped_lp_mesh_hop;
    }
    return candidate.mesh_hop_sum + 1.0e-9 < best.mesh_hop_sum;
  }

  PlacementState PolishCostAwarePRISAPareto(
      PlacementState state, std::chrono::steady_clock::time_point start) {
    const int resource_count = static_cast<int>(prisa_cell_order_.size());
    if (resource_count < 2) return state;

    CostAwarePRISAMetrics current = ComputeCostAwarePRISAMetrics(state);
    const int passes = std::min(4, PRISAPolishPasses());
    for (int pass = 0; pass < passes && !HasTimedOut(start, 0.05); pass++) {
      bool improved = false;
      PlacementState best_state = state;
      CostAwarePRISAMetrics best_metrics = current;
      const int current_weak_edges = PRISAWeakEdgeCount(state);

      for (int first_index = 0; first_index < resource_count; first_index++) {
        for (int second_index = first_index + 1;
             second_index < resource_count && !HasTimedOut(start, 0.05);
             second_index++) {
          const int first_cell = prisa_cell_order_[first_index];
          const int second_cell = prisa_cell_order_[second_index];
          if (!CanSwapCells(state, first_cell, second_cell)) continue;

          PlacementState next = state;
          if (!ApplyCellSwap(next, first_cell, second_cell)) continue;
          if (PRISAWeakEdgeCount(next) > current_weak_edges) continue;
          const CostAwarePRISAMetrics next_metrics =
              ComputeCostAwarePRISAMetrics(next);
          if (!IsCostAwareParetoImprovement(next_metrics, current)) continue;
          if (!improved ||
              IsBetterCostAwareParetoCandidate(next_metrics, best_metrics)) {
            best_state = std::move(next);
            best_metrics = next_metrics;
            improved = true;
          }
        }
      }

      if (!improved) break;
      state = std::move(best_state);
      current = best_metrics;
    }
    return state;
  }

  std::optional<PlacementState> RunPRISA(
      std::chrono::steady_clock::time_point start) {
    auto current = ConstructPRISAInitialPlacement();
    if (!current.has_value()) return std::nullopt;
    PRISAStats stats = InitializePRISAStats(*current);

    PlacementState best = *current;
    double current_cost = PRISAAcceptanceScore(*current);
    double best_cost = current_cost;
    PlacementQuality best_quality = ComputePlacementQuality(best);
    const double initial_cost = PlacementCost(*current);
    const double initial_acceptance_cost = current_cost;
    const int initial_weak_edges = stats.weak_edges;
    const int max_iterations = PRISAMaxIterations();
    const double start_probability = UsesPRISASIS() ? 0.20 : 0.40;
    const double end_probability = 0.01;
    double temperature = -1.0 / std::log(start_probability);
    const double final_temperature = -1.0 / std::log(end_probability);
    const double cooling =
        std::pow(final_temperature / temperature,
                 1.0 / std::max(1, max_iterations));
    std::uniform_real_distribution<double> unit(0.0, 1.0);
    int generated_moves = 0;
    int accepted_moves = 0;
    int improving_moves = 0;
    int completed_iterations = 0;

    for (int iteration = 0;
         iteration < max_iterations && !HasTimedOut(start, 0.05);
         iteration++) {
      completed_iterations = iteration + 1;
      auto proposal = ProposePRISAMove(*current, stats);
      if (!proposal.has_value()) continue;
      generated_moves++;
      const double next_cost = current_cost + proposal->cost_delta;
      const double delta = next_cost - current_cost;
      const bool accept =
          delta <= 0.0 || unit(rng_) < std::exp(-delta / temperature);
      if (accept) {
        if (!ApplyCellSwapWithStats(*current, stats, proposal->first_cell,
                                    proposal->second_cell)) {
          continue;
        }
        current_cost = next_cost;
        accepted_moves++;
        const bool better_cost = current_cost + 1.0e-9 < best_cost;
        bool better_same_cost = false;
        if (!UsesCostAwarePRISA() &&
            std::abs(current_cost - best_cost) <= 1.0e-9) {
          const PlacementQuality current_quality =
              ComputePlacementQuality(*current);
          better_same_cost = IsBetterQuality(current_quality, best_quality);
          if (better_same_cost) best_quality = current_quality;
        }
        if (better_cost || better_same_cost) {
          best = *current;
          best_cost = current_cost;
          if (better_cost) best_quality = ComputePlacementQuality(best);
          improving_moves++;
        }
      }
      temperature = std::max(final_temperature, temperature * cooling);
    }

    if (UsesCostAwarePRISA()) {
      const double before_polish_cost = best_cost;
      PlacementState polished = PolishPRISAQuality(best, start);
      polished = PolishCostAwarePRISADirectEdges(polished, start);
      polished = PolishCostAwarePRISAPareto(polished, start);
      const double polished_cost = PRISAAcceptanceScore(polished);
      const CostAwarePRISAMetrics polished_metrics =
          ComputeCostAwarePRISAMetrics(polished);
      const CostAwarePRISAMetrics best_metrics =
          ComputeCostAwarePRISAMetrics(best);
      if (polished_cost + 1.0e-9 < before_polish_cost ||
          (polished_metrics.direct_edge_count > best_metrics.direct_edge_count &&
           polished_metrics.max_mesh_hop <= best_metrics.max_mesh_hop &&
           polished_metrics.mapped_lp_mesh_hop <=
               best_metrics.mapped_lp_mesh_hop)) {
        best = std::move(polished);
        best_cost = PRISAAcceptanceScore(best);
        improving_moves++;
      }
    }

    Log("array_prisa_result sis=" + std::to_string(UsesPRISASIS() ? 1 : 0) +
        " iterations=" + std::to_string(completed_iterations) +
        " generated_moves=" + std::to_string(generated_moves) +
        " accepted_moves=" + std::to_string(accepted_moves) +
        " improving_moves=" + std::to_string(improving_moves) +
        " initial_cost=" + std::to_string(initial_cost) +
        " initial_acceptance_cost=" +
        std::to_string(initial_acceptance_cost) +
        " best_cost=" + std::to_string(best_cost) +
        " best_placement_cost=" + std::to_string(PlacementCost(best)) +
        " initial_weak_edges=" + std::to_string(initial_weak_edges) +
        " best_weak_edges=" + std::to_string(stats.weak_edges));
    return best;
  }

  std::optional<PlacementState> RunPRISAMultiSeed(
      std::chrono::steady_clock::time_point start) {
    std::optional<PlacementState> best;
    double best_cost = std::numeric_limits<double>::infinity();
    PlacementQuality best_quality;
    int seeds = 0;
    for (int seed = 0; seed < SeedCount() && !HasTimedOut(start, 0.05);
         seed++) {
      ResetSeed(seed);
      auto placement = RunPRISA(start);
      seeds++;
      if (!placement.has_value()) continue;
      const double cost = PRISAAcceptanceScore(*placement);
      const PlacementQuality quality = ComputePlacementQuality(*placement);
      const bool better_cost = cost + 1.0e-9 < best_cost;
      const bool better_same_cost =
          !UsesCostAwarePRISA() && std::abs(cost - best_cost) <= 1.0e-9 &&
          IsBetterQuality(quality, best_quality);
      if (better_cost || better_same_cost) {
        best = placement;
        best_cost = cost;
        best_quality = quality;
      }
    }
    Log("array_prisa_search seeds=" + std::to_string(seeds) +
        " sis=" + std::to_string(UsesPRISASIS() ? 1 : 0));
    return best;
  }

  // Internal SA baseline used as a heuristic comparison point.
  bool ApplySwap(PlacementState& state, int a, int b) {
    if (a == b) return false;
    const int cell_a = state.dfg_to_cell[a];
    const int cell_b = state.dfg_to_cell[b];
    if (cell_a < 0 || cell_b < 0) return false;
    if (!IsCompatible(a, cell_b) || !IsCompatible(b, cell_a)) return false;
    state.dfg_to_cell[a] = cell_b;
    state.dfg_to_cell[b] = cell_a;
    state.cell_to_dfg[cell_a] = b;
    state.cell_to_dfg[cell_b] = a;
    return true;
  }

  bool ApplyMoveToFreeCell(PlacementState& state, int node) {
    auto cells = CompatibleCells(node, state);
    if (cells.empty()) return false;
    std::uniform_int_distribution<int> dist(0, static_cast<int>(cells.size()) - 1);
    const int old_cell = state.dfg_to_cell[node];
    const int new_cell = cells[dist(rng_)];
    state.cell_to_dfg[old_cell] = -1;
    state.cell_to_dfg[new_cell] = node;
    state.dfg_to_cell[node] = new_cell;
    return true;
  }

  std::optional<PlacementState> RunSA(std::chrono::steady_clock::time_point start) {
    auto current = RandomPlacement();
    if (!current.has_value()) return std::nullopt;
    PlacementState best = *current;
    double current_cost = PlacementCost(*current);
    double best_cost = current_cost;
    double temperature = std::max(10.0, current_cost / std::max(1, dfg_.GetNodeNum()));
    std::uniform_real_distribution<double> unit(0.0, 1.0);
    std::uniform_int_distribution<int> node_dist(0, std::max(0, dfg_.GetNodeNum() - 1));

    for (int iteration = 0;
         iteration < MaxIterations() && !HasTimedOut(start, 0.05);
         iteration++) {
      PlacementState next = *current;
      bool changed = false;
      if (unit(rng_) < 0.75 || dfg_.GetNodeNum() < rows_ * cols_) {
        changed = ApplySwap(next, node_dist(rng_), node_dist(rng_));
      } else {
        changed = ApplyMoveToFreeCell(next, node_dist(rng_));
      }
      if (!changed) continue;
      const double next_cost = PlacementCost(next);
      const double delta = next_cost - current_cost;
      if (delta <= 0.0 || unit(rng_) < std::exp(-delta / temperature)) {
        *current = next;
        current_cost = next_cost;
      }
      if (next_cost < best_cost) {
        best = next;
        best_cost = next_cost;
      }
      temperature *= 0.9975;
      if (temperature < 1.0e-4) temperature = std::max(1.0, best_cost / 10.0);
    }
    return best;
  }

  std::optional<PlacementState> RunSAMultiSeed(
      std::chrono::steady_clock::time_point start) {
    std::optional<PlacementState> best;
    double best_cost = std::numeric_limits<double>::infinity();
    int seeds = 0;
    for (int seed = 0; seed < SeedCount() && !HasTimedOut(start, 0.05); seed++) {
      ResetSeed(seed);
      auto placement = RunSA(start);
      seeds++;
      if (!placement.has_value()) continue;
      const double cost = PlacementCost(*placement);
      if (cost < best_cost) {
        best = placement;
        best_cost = cost;
      }
    }
    Log("array_sa seeds=" + std::to_string(seeds));
    return best;
  }
};

}  // namespace

namespace mapper::detail {

MappingResult RunPlacement2DArrayEngine(
    entity::DFG& dfg, entity::MRRG& mrrg, Placement2DArrayKind kind,
    double timeout_s, const std::optional<std::string>& log_file_path,
    std::optional<int> max_trials, std::optional<int> seed_count,
    std::optional<int> random_seed, std::optional<int> max_iterations) {
  Placement2DArrayEngine engine(dfg, mrrg, kind, timeout_s, log_file_path,
                                max_trials, seed_count, random_seed,
                                max_iterations);
  return engine.Run();
}

}  // namespace mapper::detail
