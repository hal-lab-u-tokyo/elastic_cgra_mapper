#include <mapper/placement2d/placement2d_array_mappers.hpp>

#include <mapper/mapper_factory.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <fstream>
#include <functional>
#include <limits>
#include <numeric>
#include <random>
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
  }

  mapper::MappingResult Run() {
    const auto start = std::chrono::steady_clock::now();
    Log("start mapper=" + MapperName() + " nodes=" +
        std::to_string(dfg_.GetNodeNum()) + " cells=" +
        std::to_string(rows_ * cols_) + " max_trials=" +
        std::to_string(MaxTrials()) + " seed_count=" +
        std::to_string(SeedCount()));

    std::optional<PlacementState> placement;
    if (kind_ == mapper::Placement2DArrayKind::kSA) {
      placement = RunSAMultiSeed(start);
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
  std::vector<int> degree_;
  std::vector<std::vector<int>> compatible_cells_;
  long long cell_visits_ = 0;

  std::string MapperName() const {
    switch (kind_) {
      case mapper::Placement2DArrayKind::kYOTO:
        return "Placement2DArrayYOTO";
      case mapper::Placement2DArrayKind::kYOTT:
        return "Placement2DArrayYOTT";
      case mapper::Placement2DArrayKind::kSA:
        return "Placement2DArraySA";
    }
    return "Placement2DArray";
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

  void BuildDFGCache() {
    const int node_num = dfg_.GetNodeNum();
    successors_.assign(node_num, {});
    predecessors_.assign(node_num, {});
    degree_.assign(node_num, 0);
    for (int source = 0; source < node_num; source++) {
      successors_[source] = dfg_.GetAdjacentNodeIdVec(source);
      for (int target : successors_[source]) {
        edges_.push_back({source, target});
        if (target >= 0 && target < node_num && target != source) {
          predecessors_[target].push_back(source);
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
    if (op == entity::OpType::OUTPUT || op == entity::OpType::STORE) return true;
    return op == entity::OpType::LOAD && predecessors_[dfg_node].empty();
  }

  bool CanPlace(int dfg_node, int cell, const PlacementState& state) const {
    return state.cell_to_dfg[cell] < 0 && IsCompatible(dfg_node, cell);
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
      if (kind_ == mapper::Placement2DArrayKind::kYOTO) {
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

  int ChooseCellForStep(const Step& step, PlacementState& state) {
    if (state.dfg_to_cell[step.anchor] < 0) {
      const int anchor_cell = ChooseInitialCell(step.anchor, state);
      if (anchor_cell < 0) return -1;
      PlaceNode(step.anchor, anchor_cell, state);
    }
    const int anchor_cell = state.dfg_to_cell[step.anchor];
    return ChooseBestCell(step.target, state, [&](int cell) {
      if (kind_ == mapper::Placement2DArrayKind::kYOTO) {
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

const bool kPlacement2DArrayYOTOMapperRegistered =
    mapper::RegisterMapperType<mapper::Placement2DArrayYOTOMapper>(
        "Placement2DArrayYOTOMapper");
const bool kPlacement2DArrayYOTOShortNameRegistered =
    mapper::RegisterMapperType<mapper::Placement2DArrayYOTOMapper>(
        "Placement2DArrayYOTO");
const bool kPlacement2DArrayYOTTMapperRegistered =
    mapper::RegisterMapperType<mapper::Placement2DArrayYOTTMapper>(
        "Placement2DArrayYOTTMapper");
const bool kPlacement2DArrayYOTTShortNameRegistered =
    mapper::RegisterMapperType<mapper::Placement2DArrayYOTTMapper>(
        "Placement2DArrayYOTT");
const bool kPlacement2DArraySAMapperRegistered =
    mapper::RegisterMapperType<mapper::Placement2DArraySAMapper>(
        "Placement2DArraySAMapper");
const bool kPlacement2DArraySAShortNameRegistered =
    mapper::RegisterMapperType<mapper::Placement2DArraySAMapper>(
        "Placement2DArraySA");

}  // namespace

namespace mapper {

Placement2DArrayMapperBase::Placement2DArrayMapperBase(
    Placement2DArrayKind kind, const std::shared_ptr<entity::DFG> dfg_ptr,
    const std::shared_ptr<entity::MRRG> mrrg_ptr)
    : kind_(kind), dfg_ptr_(dfg_ptr), mrrg_ptr_(mrrg_ptr) {}

MappingResult Placement2DArrayMapperBase::Execution() {
  Placement2DArrayEngine engine(*dfg_ptr_, *mrrg_ptr_, kind_,
                                timeout_s_.value_or(1.0), log_file_path_,
                                max_trials_, seed_count_, random_seed_,
                                max_iterations_);
  return engine.Run();
}

void Placement2DArrayMapperBase::SetLogFilePath(
    const std::string& log_file_path) {
  log_file_path_ = log_file_path;
}

void Placement2DArrayMapperBase::SetTimeOut(double time_out_s) {
  timeout_s_ = time_out_s;
}

void Placement2DArrayMapperBase::SetAcceptFeasibleSolution(
    bool accept_feasible_solution) {
  accept_feasible_solution_ = accept_feasible_solution;
}

void Placement2DArrayMapperBase::SetMaxTrials(int max_trials) {
  max_trials_ = max_trials;
}

void Placement2DArrayMapperBase::SetSeedCount(int seed_count) {
  seed_count_ = seed_count;
}

void Placement2DArrayMapperBase::SetRoutingRetryCount(int routing_retry_count) {
  routing_retry_count_ = routing_retry_count;
}

void Placement2DArrayMapperBase::SetRandomSeed(int random_seed) {
  random_seed_ = random_seed;
}

void Placement2DArrayMapperBase::SetMaxIterations(int max_iterations) {
  max_iterations_ = max_iterations;
}

void Placement2DArrayMapperBase::SetPlacementOnly(bool placement_only) {
  placement_only_ = placement_only;
}

}  // namespace mapper
