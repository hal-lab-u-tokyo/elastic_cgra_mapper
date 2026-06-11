#include <gurobi_c++.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <map>
#include <memory>
#include <mapper/connectivity_based_ilp_mapper.hpp>
#include <mapper/mapper_factory.hpp>
#include <optional>
#include <queue>
#include <set>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

const bool kConnectivityBasedILPMapperRegistered =
    mapper::RegisterMapperType<mapper::ConnectivityBasedILPMapper>(
        "ConnectivityBasedILPMapper");

constexpr int kRelaxedPathsPerConnection = 3;
constexpr int kRoutingPathsPerConnection = 20;
constexpr int kRelaxedPathExtraDepth = 2;
constexpr int kRoutingPathExtraDepth = 4;
constexpr int kRelaxedResourceCapacity = 2;
constexpr int kMaxRelaxedPlacements = 100;
constexpr double kEpsilonTimeLimit = 0.05;

struct DFGEdge {
  int source;
  int target;
};

struct EdgeCandidate {
  int dfg_edge_id;
  int source_dfg_node_id;
  int target_dfg_node_id;
  int source_mrrg_node_id;
  int target_mrrg_node_id;
};

struct CandidatePath {
  int edge_candidate_id;
  int dfg_edge_id;
  int source_dfg_node_id;
  int target_dfg_node_id;
  int source_mrrg_node_id;
  int target_mrrg_node_id;
  std::vector<int> edge_ids;
  std::vector<int> internal_node_ids;
};

struct ConnectivityData {
  std::vector<std::vector<int>> candidate_nodes;
  std::vector<std::unordered_map<int, int>> neighbor_distance_by_source;
  std::vector<EdgeCandidate> edge_candidates;
  std::vector<std::vector<int>> edge_candidate_ids_by_dfg_edge;
};

struct PathData {
  std::vector<CandidatePath> paths;
  std::vector<std::vector<int>> path_ids_by_edge_candidate;
  std::vector<std::vector<int>> path_ids_by_mrrg_edge;
  std::vector<std::vector<int>> path_ids_by_internal_node;
};

struct PlacementVars {
  std::vector<std::vector<GRBVar>> map_op_to_PE;
  std::vector<GRBVar> map_dfg_edge_to_fu_pair;
};

struct PlacementCandidate {
  std::vector<int> dfg_node_to_mrrg_node;
};

std::vector<int> BuildNeighborCountSchedule() {
  std::vector<int> result;
  for (int neighbor_count = 4; neighbor_count <= 24; neighbor_count += 2) {
    result.push_back(neighbor_count);
  }
  return result;
}

std::vector<DFGEdge> BuildDFGEdgeVec(entity::DFG& dfg) {
  std::vector<DFGEdge> result;
  for (int source_id = 0; source_id < dfg.GetNodeNum(); source_id++) {
    for (int target_id : dfg.GetAdjacentNodeIdVec(source_id)) {
      result.push_back({source_id, target_id});
    }
  }
  return result;
}

bool SupportsOperation(const entity::MRRGNodeProperty& mrrg_node,
                       entity::OpType op) {
  return std::find(mrrg_node.supported_operations.begin(),
                   mrrg_node.supported_operations.end(),
                   op) != mrrg_node.supported_operations.end();
}

bool SupportsAnyDFGOperation(const entity::MRRGNodeProperty& mrrg_node) {
  for (entity::OpType op : mrrg_node.supported_operations) {
    if (entity::IsDFGOp(op)) {
      return true;
    }
  }
  return false;
}

std::vector<std::vector<int>> BuildCandidateNodes(entity::DFG& dfg,
                                                  entity::MRRG& mrrg) {
  std::vector<std::vector<int>> result(dfg.GetNodeNum());
  for (int dfg_node_id = 0; dfg_node_id < dfg.GetNodeNum(); dfg_node_id++) {
    entity::OpType dfg_op = dfg.GetNodeProperty(dfg_node_id).op;
    for (int mrrg_node_id = 0; mrrg_node_id < mrrg.GetNodeNum();
         mrrg_node_id++) {
      if (SupportsOperation(mrrg.GetNodeProperty(mrrg_node_id), dfg_op)) {
        result[dfg_node_id].push_back(mrrg_node_id);
      }
    }
  }
  return result;
}

std::vector<bool> BuildFunctionalNodeFlags(entity::DFG& dfg,
                                           entity::MRRG& mrrg) {
  std::vector<bool> result(mrrg.GetNodeNum(), false);
  for (int mrrg_node_id = 0; mrrg_node_id < mrrg.GetNodeNum();
       mrrg_node_id++) {
    if (!SupportsAnyDFGOperation(mrrg.GetNodeProperty(mrrg_node_id))) {
      continue;
    }
    for (int dfg_node_id = 0; dfg_node_id < dfg.GetNodeNum(); dfg_node_id++) {
      if (SupportsOperation(mrrg.GetNodeProperty(mrrg_node_id),
                            dfg.GetNodeProperty(dfg_node_id).op)) {
        result[mrrg_node_id] = true;
        break;
      }
    }
  }
  return result;
}

std::vector<std::unordered_map<int, int>> BuildNeighborDistances(
    entity::MRRG& mrrg, const std::vector<bool>& functional_node_flags,
    int target_neighbor_count) {
  std::vector<std::unordered_map<int, int>> result(mrrg.GetNodeNum());

  for (int source_node_id = 0; source_node_id < mrrg.GetNodeNum();
       source_node_id++) {
    if (!functional_node_flags[source_node_id]) {
      continue;
    }

    std::vector<int> best_depth(mrrg.GetNodeNum(), -1);
    std::queue<int> q;
    q.push(source_node_id);
    best_depth[source_node_id] = 0;
    int stop_depth = -1;

    while (!q.empty()) {
      int current_node_id = q.front();
      q.pop();
      int current_depth = best_depth[current_node_id];
      if (stop_depth >= 0 && current_depth >= stop_depth) {
        continue;
      }

      for (int edge_id : mrrg.GetOutEdgeIdVec(current_node_id)) {
        int next_node_id = mrrg.GetEdgeSourceTarget(edge_id).second;
        if (best_depth[next_node_id] >= 0) {
          continue;
        }

        int next_depth = current_depth + 1;
        best_depth[next_node_id] = next_depth;
        q.push(next_node_id);
        if (next_node_id != source_node_id &&
            functional_node_flags[next_node_id]) {
          result[source_node_id][next_node_id] = next_depth;
          if (static_cast<int>(result[source_node_id].size()) >=
              target_neighbor_count) {
            stop_depth = next_depth;
          }
        }
      }
    }
  }

  return result;
}

std::vector<EdgeCandidate> BuildEdgeCandidates(
    const std::vector<DFGEdge>& dfg_edge_vec,
    const std::vector<std::vector<int>>& candidate_nodes,
    const std::vector<std::unordered_map<int, int>>& neighbor_distance_by_source,
    std::vector<std::vector<int>>& edge_candidate_ids_by_dfg_edge) {
  std::vector<EdgeCandidate> result;
  edge_candidate_ids_by_dfg_edge.assign(dfg_edge_vec.size(), {});

  for (int dfg_edge_id = 0;
       dfg_edge_id < static_cast<int>(dfg_edge_vec.size()); dfg_edge_id++) {
    int source_dfg_node_id = dfg_edge_vec[dfg_edge_id].source;
    int target_dfg_node_id = dfg_edge_vec[dfg_edge_id].target;

    for (int source_mrrg_node_id : candidate_nodes[source_dfg_node_id]) {
      for (int target_mrrg_node_id : candidate_nodes[target_dfg_node_id]) {
        bool is_self_edge = source_dfg_node_id == target_dfg_node_id;
        bool is_valid_pair =
            is_self_edge
                ? source_mrrg_node_id == target_mrrg_node_id
                : neighbor_distance_by_source[source_mrrg_node_id].count(
                      target_mrrg_node_id) > 0;
        if (!is_valid_pair) {
          continue;
        }

        int edge_candidate_id = static_cast<int>(result.size());
        result.push_back({dfg_edge_id, source_dfg_node_id, target_dfg_node_id,
                          source_mrrg_node_id, target_mrrg_node_id});
        edge_candidate_ids_by_dfg_edge[dfg_edge_id].push_back(edge_candidate_id);
      }
    }
  }

  return result;
}

ConnectivityData BuildConnectivityData(entity::DFG& dfg, entity::MRRG& mrrg,
                                       const std::vector<DFGEdge>& dfg_edge_vec,
                                       int neighbor_count) {
  ConnectivityData data;
  data.candidate_nodes = BuildCandidateNodes(dfg, mrrg);
  std::vector<bool> functional_node_flags = BuildFunctionalNodeFlags(dfg, mrrg);
  data.neighbor_distance_by_source =
      BuildNeighborDistances(mrrg, functional_node_flags, neighbor_count);
  data.edge_candidates = BuildEdgeCandidates(
      dfg_edge_vec, data.candidate_nodes, data.neighbor_distance_by_source,
      data.edge_candidate_ids_by_dfg_edge);
  return data;
}

bool HasRequiredCandidates(const ConnectivityData& data) {
  for (const auto& node_candidates : data.candidate_nodes) {
    if (node_candidates.empty()) {
      return false;
    }
  }
  for (const auto& edge_candidates : data.edge_candidate_ids_by_dfg_edge) {
    if (edge_candidates.empty()) {
      return false;
    }
  }
  return true;
}

bool ContainsNode(const std::vector<int>& node_ids, int node_id) {
  return std::find(node_ids.begin(), node_ids.end(), node_id) != node_ids.end();
}

std::vector<std::vector<int>> FindKShortestSimplePaths(
    entity::MRRG& mrrg, int source_mrrg_node_id, int target_mrrg_node_id,
    int max_paths, int max_path_edges) {
  std::vector<std::vector<int>> result;
  struct PathState {
    int node_id;
    std::vector<int> edge_ids;
    std::vector<int> visited_node_ids;
  };

  std::queue<PathState> q;
  q.push({source_mrrg_node_id, {}, {source_mrrg_node_id}});

  while (!q.empty() && static_cast<int>(result.size()) < max_paths) {
    PathState current = q.front();
    q.pop();
    if (static_cast<int>(current.edge_ids.size()) >= max_path_edges) {
      continue;
    }

    for (int edge_id : mrrg.GetOutEdgeIdVec(current.node_id)) {
      int next_node_id = mrrg.GetEdgeSourceTarget(edge_id).second;
      if (ContainsNode(current.visited_node_ids, next_node_id)) {
        continue;
      }

      std::vector<int> next_edge_ids = current.edge_ids;
      next_edge_ids.push_back(edge_id);
      if (next_node_id == target_mrrg_node_id) {
        result.push_back(next_edge_ids);
        if (static_cast<int>(result.size()) >= max_paths) {
          break;
        }
        continue;
      }

      std::vector<int> next_visited = current.visited_node_ids;
      next_visited.push_back(next_node_id);
      q.push({next_node_id, next_edge_ids, next_visited});
    }
  }

  return result;
}

std::vector<std::vector<int>> FindKShortestCycles(entity::MRRG& mrrg,
                                                  int source_mrrg_node_id,
                                                  int max_paths,
                                                  int max_path_edges) {
  std::vector<std::vector<int>> result;
  struct PathState {
    int node_id;
    std::vector<int> edge_ids;
    std::vector<int> visited_node_ids;
  };

  std::queue<PathState> q;
  q.push({source_mrrg_node_id, {}, {source_mrrg_node_id}});

  while (!q.empty() && static_cast<int>(result.size()) < max_paths) {
    PathState current = q.front();
    q.pop();
    if (static_cast<int>(current.edge_ids.size()) >= max_path_edges) {
      continue;
    }

    for (int edge_id : mrrg.GetOutEdgeIdVec(current.node_id)) {
      int next_node_id = mrrg.GetEdgeSourceTarget(edge_id).second;
      std::vector<int> next_edge_ids = current.edge_ids;
      next_edge_ids.push_back(edge_id);
      if (next_node_id == source_mrrg_node_id) {
        result.push_back(next_edge_ids);
        if (static_cast<int>(result.size()) >= max_paths) {
          break;
        }
        continue;
      }
      if (ContainsNode(current.visited_node_ids, next_node_id)) {
        continue;
      }

      std::vector<int> next_visited = current.visited_node_ids;
      next_visited.push_back(next_node_id);
      q.push({next_node_id, next_edge_ids, next_visited});
    }
  }

  return result;
}

std::vector<int> BuildInternalNodeIds(entity::MRRG& mrrg,
                                      const CandidatePath& path) {
  std::set<int> internal_node_set;
  for (int edge_id : path.edge_ids) {
    int to_node_id = mrrg.GetEdgeSourceTarget(edge_id).second;
    if (to_node_id == path.source_mrrg_node_id ||
        to_node_id == path.target_mrrg_node_id) {
      continue;
    }
    internal_node_set.insert(to_node_id);
  }
  return std::vector<int>(internal_node_set.begin(), internal_node_set.end());
}

PathData BuildPathData(
    entity::MRRG& mrrg, const std::vector<EdgeCandidate>& edge_candidates,
    const std::vector<std::unordered_map<int, int>>& neighbor_distance_by_source,
    int paths_per_connection, int extra_depth) {
  PathData data;
  data.path_ids_by_edge_candidate.assign(edge_candidates.size(), {});
  data.path_ids_by_mrrg_edge.assign(mrrg.GetEdgeNum(), {});
  data.path_ids_by_internal_node.assign(mrrg.GetNodeNum(), {});

  const int self_loop_depth =
      std::max(1, mrrg.GetMRRGConfig().context_size + extra_depth);

  for (int edge_candidate_id = 0;
       edge_candidate_id < static_cast<int>(edge_candidates.size());
       edge_candidate_id++) {
    const EdgeCandidate& edge_candidate = edge_candidates[edge_candidate_id];
    std::vector<std::vector<int>> path_edge_id_vecs;
    if (edge_candidate.source_mrrg_node_id ==
        edge_candidate.target_mrrg_node_id) {
      path_edge_id_vecs =
          FindKShortestCycles(mrrg, edge_candidate.source_mrrg_node_id,
                              paths_per_connection, self_loop_depth);
    } else {
      auto distance_it =
          neighbor_distance_by_source[edge_candidate.source_mrrg_node_id].find(
              edge_candidate.target_mrrg_node_id);
      if (distance_it == neighbor_distance_by_source
                             [edge_candidate.source_mrrg_node_id]
                                 .end()) {
        continue;
      }
      int max_path_edges = distance_it->second + extra_depth;
      path_edge_id_vecs = FindKShortestSimplePaths(
          mrrg, edge_candidate.source_mrrg_node_id,
          edge_candidate.target_mrrg_node_id, paths_per_connection,
          max_path_edges);
    }

    for (const auto& path_edge_ids : path_edge_id_vecs) {
      CandidatePath path{edge_candidate_id,
                         edge_candidate.dfg_edge_id,
                         edge_candidate.source_dfg_node_id,
                         edge_candidate.target_dfg_node_id,
                         edge_candidate.source_mrrg_node_id,
                         edge_candidate.target_mrrg_node_id,
                         path_edge_ids,
                         {}};
      path.internal_node_ids = BuildInternalNodeIds(mrrg, path);

      int path_id = static_cast<int>(data.paths.size());
      data.paths.push_back(path);
      data.path_ids_by_edge_candidate[edge_candidate_id].push_back(path_id);
      for (int edge_id : path.edge_ids) {
        data.path_ids_by_mrrg_edge[edge_id].push_back(path_id);
      }
      for (int node_id : path.internal_node_ids) {
        data.path_ids_by_internal_node[node_id].push_back(path_id);
      }
    }
  }

  return data;
}

double ElapsedSeconds(std::chrono::system_clock::time_point start_time) {
  const auto end_time = std::chrono::system_clock::now();
  return std::chrono::duration_cast<std::chrono::milliseconds>(end_time -
                                                               start_time)
             .count() /
         1000.0;
}

bool ConfigureModel(GRBModel& model, const std::optional<std::string>& log_file,
                    const std::optional<double>& timeout_s,
                    std::chrono::system_clock::time_point start_time) {
  if (log_file.has_value()) {
    model.set("LogFile", log_file.value());
  }
  if (!timeout_s.has_value()) {
    return true;
  }

  double remaining_s = timeout_s.value() - ElapsedSeconds(start_time);
  if (remaining_s <= 0.0) {
    return false;
  }
  model.set(GRB_DoubleParam_TimeLimit,
            std::max(kEpsilonTimeLimit, remaining_s));
  return true;
}

bool HasUsableSolution(GRBModel& model, bool accept_feasible_solution) {
  int status = model.get(GRB_IntAttr_Status);
  if (status == GRB_INFEASIBLE || status == GRB_INF_OR_UNBD ||
      status == GRB_UNBOUNDED) {
    return false;
  }
  if (model.get(GRB_IntAttr_SolCount) == 0) {
    return false;
  }
  if (!accept_feasible_solution && status != GRB_OPTIMAL &&
      status != GRB_SUBOPTIMAL) {
    return false;
  }
  return true;
}

PlacementVars AddPlacementAndConnectivityConstraints(
    GRBModel& model, entity::DFG& dfg, entity::MRRG& mrrg,
    const ConnectivityData& data, const std::vector<DFGEdge>& dfg_edge_vec) {
  const int dfg_node_num = dfg.GetNodeNum();
  const int mrrg_node_num = mrrg.GetNodeNum();
  const double coefficient = 1.0;

  PlacementVars vars{
      std::vector<std::vector<GRBVar>>(
          dfg_node_num, std::vector<GRBVar>(mrrg_node_num)),
      std::vector<GRBVar>(data.edge_candidates.size())};

  for (int dfg_node_id = 0; dfg_node_id < dfg_node_num; dfg_node_id++) {
    for (int mrrg_node_id = 0; mrrg_node_id < mrrg_node_num; mrrg_node_id++) {
      vars.map_op_to_PE[dfg_node_id][mrrg_node_id] =
          model.addVar(0.0, 1.0, 0.0, GRB_BINARY,
                       "f_" + std::to_string(dfg_node_id) + "_" +
                           std::to_string(mrrg_node_id));
    }
  }
  for (int edge_candidate_id = 0;
       edge_candidate_id < static_cast<int>(data.edge_candidates.size());
       edge_candidate_id++) {
    vars.map_dfg_edge_to_fu_pair[edge_candidate_id] =
        model.addVar(0.0, 1.0, 0.0, GRB_BINARY,
                     "e_" + std::to_string(edge_candidate_id));
  }

  for (int dfg_node_id = 0; dfg_node_id < dfg_node_num; dfg_node_id++) {
    GRBLinExpr placement_expr;
    for (int mrrg_node_id = 0; mrrg_node_id < mrrg_node_num; mrrg_node_id++) {
      placement_expr.addTerms(&coefficient,
                              &(vars.map_op_to_PE[dfg_node_id][mrrg_node_id]),
                              1);
      if (!SupportsOperation(mrrg.GetNodeProperty(mrrg_node_id),
                             dfg.GetNodeProperty(dfg_node_id).op)) {
        model.addConstr(vars.map_op_to_PE[dfg_node_id][mrrg_node_id],
                        GRB_EQUAL, 0,
                        "c_legality_" + std::to_string(dfg_node_id) + "_" +
                            std::to_string(mrrg_node_id));
      }
    }
    model.addConstr(placement_expr, GRB_EQUAL, 1,
                    "c_placement_" + std::to_string(dfg_node_id));
  }

  for (int mrrg_node_id = 0; mrrg_node_id < mrrg_node_num; mrrg_node_id++) {
    GRBLinExpr node_occupancy;
    for (int dfg_node_id = 0; dfg_node_id < dfg_node_num; dfg_node_id++) {
      node_occupancy.addTerms(
          &coefficient, &(vars.map_op_to_PE[dfg_node_id][mrrg_node_id]), 1);
    }
    model.addConstr(node_occupancy, GRB_LESS_EQUAL, 1,
                    "c_fu_exclusivity_" + std::to_string(mrrg_node_id));
  }

  for (int edge_candidate_id = 0;
       edge_candidate_id < static_cast<int>(data.edge_candidates.size());
       edge_candidate_id++) {
    const EdgeCandidate& edge_candidate = data.edge_candidates[edge_candidate_id];
    model.addConstr(
        vars.map_dfg_edge_to_fu_pair[edge_candidate_id], GRB_LESS_EQUAL,
        vars.map_op_to_PE[edge_candidate.source_dfg_node_id]
                         [edge_candidate.source_mrrg_node_id],
        "c_edge_source_" + std::to_string(edge_candidate_id));
    model.addConstr(
        vars.map_dfg_edge_to_fu_pair[edge_candidate_id], GRB_LESS_EQUAL,
        vars.map_op_to_PE[edge_candidate.target_dfg_node_id]
                         [edge_candidate.target_mrrg_node_id],
        "c_edge_target_" + std::to_string(edge_candidate_id));
  }

  for (int dfg_edge_id = 0;
       dfg_edge_id < static_cast<int>(dfg_edge_vec.size()); dfg_edge_id++) {
    GRBLinExpr edge_expr;
    for (int edge_candidate_id :
         data.edge_candidate_ids_by_dfg_edge[dfg_edge_id]) {
      edge_expr.addTerms(
          &coefficient,
          &(vars.map_dfg_edge_to_fu_pair[edge_candidate_id]), 1);
    }
    model.addConstr(edge_expr, GRB_EQUAL, 1,
                    "c_dfg_edge_pair_" + std::to_string(dfg_edge_id));
  }

  return vars;
}

std::optional<PlacementCandidate> ExtractPlacement(
    GRBModel& model, const PlacementVars& vars, int dfg_node_num,
    int mrrg_node_num) {
  PlacementCandidate placement{{std::vector<int>(dfg_node_num, -1)}};

  for (int dfg_node_id = 0; dfg_node_id < dfg_node_num; dfg_node_id++) {
    std::unique_ptr<double[]> placement_values(
        model.get(GRB_DoubleAttr_X, vars.map_op_to_PE[dfg_node_id].data(),
                  mrrg_node_num));
    for (int mrrg_node_id = 0; mrrg_node_id < mrrg_node_num; mrrg_node_id++) {
      if (placement_values[mrrg_node_id] > 0.5) {
        placement.dfg_node_to_mrrg_node[dfg_node_id] = mrrg_node_id;
      }
    }
    if (placement.dfg_node_to_mrrg_node[dfg_node_id] < 0) {
      return std::nullopt;
    }
  }

  return placement;
}

void AddNoGoodPlacementConstraints(GRBModel& model, const PlacementVars& vars,
                                   const std::vector<PlacementCandidate>&
                                       rejected_placements) {
  const double coefficient = 1.0;
  for (int rejected_id = 0;
       rejected_id < static_cast<int>(rejected_placements.size());
       rejected_id++) {
    const auto& placement =
        rejected_placements[rejected_id].dfg_node_to_mrrg_node;
    GRBLinExpr same_assignment_count;
    for (int dfg_node_id = 0; dfg_node_id < static_cast<int>(placement.size());
         dfg_node_id++) {
      same_assignment_count.addTerms(
          &coefficient, &(vars.map_op_to_PE[dfg_node_id][placement[dfg_node_id]]),
          1);
    }
    model.addConstr(same_assignment_count, GRB_LESS_EQUAL,
                    static_cast<int>(placement.size()) - 1,
                    "c_no_good_placement_" + std::to_string(rejected_id));
  }
}

bool SolvePlacementOnly(
    entity::DFG& dfg, entity::MRRG& mrrg, const ConnectivityData& data,
    const std::vector<DFGEdge>& dfg_edge_vec,
    const std::optional<std::string>& log_file,
    const std::optional<double>& timeout_s, bool accept_feasible_solution,
    std::chrono::system_clock::time_point start_time) {
  GRBEnv env = GRBEnv(true);
  env.start();
  GRBModel model = GRBModel(env);
  if (!ConfigureModel(model, log_file, timeout_s, start_time)) {
    return false;
  }

  AddPlacementAndConnectivityConstraints(model, dfg, mrrg, data, dfg_edge_vec);
  model.optimize();
  return HasUsableSolution(model, accept_feasible_solution);
}

std::optional<PlacementCandidate> SolveRelaxedPlacement(
    entity::DFG& dfg, entity::MRRG& mrrg, const ConnectivityData& data,
    const PathData& path_data, const std::vector<DFGEdge>& dfg_edge_vec,
    const std::vector<PlacementCandidate>& rejected_placements,
    const std::optional<std::string>& log_file,
    const std::optional<double>& timeout_s, bool accept_feasible_solution,
    std::chrono::system_clock::time_point start_time) {
  GRBEnv env = GRBEnv(true);
  env.start();
  GRBModel model = GRBModel(env);
  if (!ConfigureModel(model, log_file, timeout_s, start_time)) {
    return std::nullopt;
  }

  PlacementVars placement_vars =
      AddPlacementAndConnectivityConstraints(model, dfg, mrrg, data,
                                             dfg_edge_vec);
  AddNoGoodPlacementConstraints(model, placement_vars, rejected_placements);

  std::vector<GRBVar> select_path(path_data.paths.size());
  for (int path_id = 0; path_id < static_cast<int>(path_data.paths.size());
       path_id++) {
    select_path[path_id] =
        model.addVar(0.0, 1.0, 0.0, GRB_BINARY,
                     "p_" + std::to_string(path_id));
  }

  const double coefficient = 1.0;
  GRBLinExpr objective;
  for (int path_id = 0; path_id < static_cast<int>(path_data.paths.size());
       path_id++) {
    double path_cost = static_cast<double>(path_data.paths[path_id].edge_ids.size());
    objective.addTerms(&path_cost, &(select_path[path_id]), 1);
  }
  model.setObjective(objective, GRB_MINIMIZE);

  for (int edge_candidate_id = 0;
       edge_candidate_id < static_cast<int>(data.edge_candidates.size());
       edge_candidate_id++) {
    GRBLinExpr path_selection;
    for (int path_id : path_data.path_ids_by_edge_candidate[edge_candidate_id]) {
      path_selection.addTerms(&coefficient, &(select_path[path_id]), 1);
      model.addConstr(select_path[path_id], GRB_LESS_EQUAL,
                      placement_vars.map_dfg_edge_to_fu_pair[edge_candidate_id],
                      "c_path_implies_edge_" + std::to_string(path_id));
    }
    model.addConstr(path_selection, GRB_EQUAL,
                    placement_vars.map_dfg_edge_to_fu_pair[edge_candidate_id],
                    "c_edge_has_path_" + std::to_string(edge_candidate_id));
  }

  for (int mrrg_edge_id = 0; mrrg_edge_id < mrrg.GetEdgeNum();
       mrrg_edge_id++) {
    GRBLinExpr edge_occupancy;
    for (int path_id : path_data.path_ids_by_mrrg_edge[mrrg_edge_id]) {
      edge_occupancy.addTerms(&coefficient, &(select_path[path_id]), 1);
    }
    model.addConstr(edge_occupancy, GRB_LESS_EQUAL, kRelaxedResourceCapacity,
                    "c_relaxed_edge_capacity_" +
                        std::to_string(mrrg_edge_id));
  }

  for (int mrrg_node_id = 0; mrrg_node_id < mrrg.GetNodeNum();
       mrrg_node_id++) {
    GRBLinExpr node_occupancy;
    for (int dfg_node_id = 0; dfg_node_id < dfg.GetNodeNum(); dfg_node_id++) {
      node_occupancy.addTerms(
          &coefficient,
          &(placement_vars.map_op_to_PE[dfg_node_id][mrrg_node_id]), 1);
    }
    for (int path_id : path_data.path_ids_by_internal_node[mrrg_node_id]) {
      node_occupancy.addTerms(&coefficient, &(select_path[path_id]), 1);
    }
    model.addConstr(node_occupancy, GRB_LESS_EQUAL,
                    kRelaxedResourceCapacity,
                    "c_relaxed_node_capacity_" +
                        std::to_string(mrrg_node_id));
  }

  model.optimize();
  if (!HasUsableSolution(model, accept_feasible_solution)) {
    return std::nullopt;
  }

  return ExtractPlacement(model, placement_vars, dfg.GetNodeNum(),
                          mrrg.GetNodeNum());
}

PathData BuildRoutingPathDataForPlacement(
    entity::MRRG& mrrg, const std::vector<DFGEdge>& dfg_edge_vec,
    const std::vector<int>& dfg_node_to_mrrg_node,
    const std::vector<std::unordered_map<int, int>>& neighbor_distance_by_source,
    std::vector<EdgeCandidate>& fixed_edge_candidates) {
  fixed_edge_candidates.clear();
  for (int dfg_edge_id = 0;
       dfg_edge_id < static_cast<int>(dfg_edge_vec.size()); dfg_edge_id++) {
    int source_dfg_node_id = dfg_edge_vec[dfg_edge_id].source;
    int target_dfg_node_id = dfg_edge_vec[dfg_edge_id].target;
    fixed_edge_candidates.push_back(
        {dfg_edge_id, source_dfg_node_id, target_dfg_node_id,
         dfg_node_to_mrrg_node[source_dfg_node_id],
         dfg_node_to_mrrg_node[target_dfg_node_id]});
  }

  return BuildPathData(mrrg, fixed_edge_candidates, neighbor_distance_by_source,
                       kRoutingPathsPerConnection, kRoutingPathExtraDepth);
}

std::optional<entity::Mapping> SolveRoutingOnly(
    entity::DFG& dfg, entity::MRRG& mrrg, const std::vector<DFGEdge>& dfg_edge_vec,
    const ConnectivityData& data, const PlacementCandidate& placement,
    const std::optional<std::string>& log_file,
    const std::optional<double>& timeout_s, bool accept_feasible_solution,
    std::chrono::system_clock::time_point start_time) {
  std::vector<EdgeCandidate> fixed_edge_candidates;
  PathData path_data = BuildRoutingPathDataForPlacement(
      mrrg, dfg_edge_vec, placement.dfg_node_to_mrrg_node,
      data.neighbor_distance_by_source, fixed_edge_candidates);

  for (const auto& path_ids : path_data.path_ids_by_edge_candidate) {
    if (path_ids.empty()) {
      return std::nullopt;
    }
  }

  GRBEnv env = GRBEnv(true);
  env.start();
  GRBModel model = GRBModel(env);
  if (!ConfigureModel(model, log_file, timeout_s, start_time)) {
    return std::nullopt;
  }

  std::vector<GRBVar> select_path(path_data.paths.size());
  for (int path_id = 0; path_id < static_cast<int>(path_data.paths.size());
       path_id++) {
    select_path[path_id] =
        model.addVar(0.0, 1.0, 0.0, GRB_BINARY,
                     "route_p_" + std::to_string(path_id));
  }

  const double coefficient = 1.0;
  GRBLinExpr objective;
  for (int path_id = 0; path_id < static_cast<int>(path_data.paths.size());
       path_id++) {
    double path_cost = static_cast<double>(path_data.paths[path_id].edge_ids.size());
    objective.addTerms(&path_cost, &(select_path[path_id]), 1);
  }
  model.setObjective(objective, GRB_MINIMIZE);

  for (int edge_candidate_id = 0;
       edge_candidate_id < static_cast<int>(fixed_edge_candidates.size());
       edge_candidate_id++) {
    GRBLinExpr selected_path_count;
    for (int path_id : path_data.path_ids_by_edge_candidate[edge_candidate_id]) {
      selected_path_count.addTerms(&coefficient, &(select_path[path_id]), 1);
    }
    model.addConstr(selected_path_count, GRB_EQUAL, 1,
                    "c_route_one_path_" + std::to_string(edge_candidate_id));
  }

  for (int mrrg_edge_id = 0; mrrg_edge_id < mrrg.GetEdgeNum();
       mrrg_edge_id++) {
    std::map<int, std::vector<int>> path_ids_by_source_mrrg;
    for (int path_id : path_data.path_ids_by_mrrg_edge[mrrg_edge_id]) {
      path_ids_by_source_mrrg[path_data.paths[path_id].source_mrrg_node_id]
          .push_back(path_id);
    }

    GRBLinExpr edge_owner_count;
    int owner_id = 0;
    for (const auto& entry : path_ids_by_source_mrrg) {
      GRBVar owner =
          model.addVar(0.0, 1.0, 0.0, GRB_BINARY,
                       "route_edge_owner_" + std::to_string(mrrg_edge_id) +
                           "_" + std::to_string(owner_id));
      GRBLinExpr source_path_count;
      for (int path_id : entry.second) {
        model.addConstr(select_path[path_id], GRB_LESS_EQUAL, owner,
                        "c_route_edge_owner_lb_" + std::to_string(path_id) +
                            "_" + std::to_string(mrrg_edge_id));
        source_path_count.addTerms(&coefficient, &(select_path[path_id]), 1);
      }
      model.addConstr(owner, GRB_LESS_EQUAL, source_path_count,
                      "c_route_edge_owner_ub_" +
                          std::to_string(mrrg_edge_id) + "_" +
                          std::to_string(owner_id));
      edge_owner_count.addTerms(&coefficient, &owner, 1);
      owner_id++;
    }
    model.addConstr(edge_owner_count, GRB_LESS_EQUAL, 1,
                    "c_route_edge_exclusivity_" +
                        std::to_string(mrrg_edge_id));
  }

  std::vector<int> mrrg_node_operation_occupancy(mrrg.GetNodeNum(), 0);
  for (int node_id : placement.dfg_node_to_mrrg_node) {
    mrrg_node_operation_occupancy[node_id] = 1;
  }

  for (int mrrg_node_id = 0; mrrg_node_id < mrrg.GetNodeNum();
       mrrg_node_id++) {
    std::map<int, std::vector<int>> path_ids_by_source_mrrg;
    for (int path_id : path_data.path_ids_by_internal_node[mrrg_node_id]) {
      path_ids_by_source_mrrg[path_data.paths[path_id].source_mrrg_node_id]
          .push_back(path_id);
    }

    GRBLinExpr node_owner_count;
    int owner_id = 0;
    for (const auto& entry : path_ids_by_source_mrrg) {
      GRBVar owner =
          model.addVar(0.0, 1.0, 0.0, GRB_BINARY,
                       "route_node_owner_" + std::to_string(mrrg_node_id) +
                           "_" + std::to_string(owner_id));
      GRBLinExpr source_path_count;
      for (int path_id : entry.second) {
        model.addConstr(select_path[path_id], GRB_LESS_EQUAL, owner,
                        "c_route_node_owner_lb_" + std::to_string(path_id) +
                            "_" + std::to_string(mrrg_node_id));
        source_path_count.addTerms(&coefficient, &(select_path[path_id]), 1);
      }
      model.addConstr(owner, GRB_LESS_EQUAL, source_path_count,
                      "c_route_node_owner_ub_" +
                          std::to_string(mrrg_node_id) + "_" +
                          std::to_string(owner_id));
      node_owner_count.addTerms(&coefficient, &owner, 1);
      owner_id++;
    }
    model.addConstr(node_owner_count, GRB_LESS_EQUAL,
                    1 - mrrg_node_operation_occupancy[mrrg_node_id],
                    "c_route_node_exclusivity_" +
                        std::to_string(mrrg_node_id));
  }

  model.optimize();
  if (!HasUsableSolution(model, accept_feasible_solution)) {
    return std::nullopt;
  }

  std::vector<std::vector<int>> dfg_output_to_mrrg_edge(dfg.GetNodeNum());
  std::unique_ptr<double[]> path_values(
      model.get(GRB_DoubleAttr_X, select_path.data(),
                static_cast<int>(select_path.size())));
  for (int path_id = 0; path_id < static_cast<int>(path_data.paths.size());
       path_id++) {
    if (path_values[path_id] <= 0.5) {
      continue;
    }
    const CandidatePath& path = path_data.paths[path_id];
    auto& route_edges = dfg_output_to_mrrg_edge[path.source_dfg_node_id];
    for (int edge_id : path.edge_ids) {
      if (std::find(route_edges.begin(), route_edges.end(), edge_id) ==
          route_edges.end()) {
        route_edges.push_back(edge_id);
      }
    }
  }

  return entity::GenerateMappingFromRoutingResult(
      mrrg, dfg, placement.dfg_node_to_mrrg_node, dfg_output_to_mrrg_edge);
}

}  // namespace

mapper::ConnectivityBasedILPMapper::ConnectivityBasedILPMapper(
    const std::shared_ptr<entity::DFG> dfg_ptr,
    const std::shared_ptr<entity::MRRG> mrrg_ptr)
    : dfg_ptr_(dfg_ptr), mrrg_ptr_(mrrg_ptr) {}

mapper::ConnectivityBasedILPMapper*
mapper::ConnectivityBasedILPMapper::CreateMapper(
    const std::shared_ptr<entity::DFG> dfg_ptr,
    const std::shared_ptr<entity::MRRG> mrrg_ptr) {
  auto* result = new mapper::ConnectivityBasedILPMapper;
  *result = mapper::ConnectivityBasedILPMapper(dfg_ptr, mrrg_ptr);
  return result;
}

mapper::MappingResult mapper::ConnectivityBasedILPMapper::Execution() {
  const auto start_time = std::chrono::system_clock::now();
  entity::Mapping empty_mapping(mrrg_ptr_->GetMRRGConfig());

  try {
    std::vector<DFGEdge> dfg_edge_vec = BuildDFGEdgeVec(*dfg_ptr_);
    std::vector<int> neighbor_count_schedule = BuildNeighborCountSchedule();

    for (int neighbor_count : neighbor_count_schedule) {
      ConnectivityData data =
          BuildConnectivityData(*dfg_ptr_, *mrrg_ptr_, dfg_edge_vec,
                                neighbor_count);
      if (!HasRequiredCandidates(data)) {
        continue;
      }

      bool placement_only_success = SolvePlacementOnly(
          *dfg_ptr_, *mrrg_ptr_, data, dfg_edge_vec, log_file_path_,
          timeout_s_, accept_feasible_solution_, start_time);
      if (!placement_only_success) {
        continue;
      }

      PathData relaxed_path_data = BuildPathData(
          *mrrg_ptr_, data.edge_candidates, data.neighbor_distance_by_source,
          kRelaxedPathsPerConnection, kRelaxedPathExtraDepth);

      std::vector<PlacementCandidate> rejected_placements;
      for (int placement_attempt = 0;
           placement_attempt < kMaxRelaxedPlacements; placement_attempt++) {
        std::optional<PlacementCandidate> placement = SolveRelaxedPlacement(
            *dfg_ptr_, *mrrg_ptr_, data, relaxed_path_data, dfg_edge_vec,
            rejected_placements, log_file_path_, timeout_s_,
            accept_feasible_solution_, start_time);
        if (!placement.has_value()) {
          break;
        }

        std::optional<entity::Mapping> routed_mapping = SolveRoutingOnly(
            *dfg_ptr_, *mrrg_ptr_, dfg_edge_vec, data, placement.value(),
            log_file_path_, timeout_s_, accept_feasible_solution_, start_time);
        if (routed_mapping.has_value()) {
          return MappingResult(true, routed_mapping.value(),
                               ElapsedSeconds(start_time));
        }

        rejected_placements.push_back(placement.value());
      }
    }

    return MappingResult(false, empty_mapping, ElapsedSeconds(start_time));
  } catch (GRBException e) {
    std::cout << "Error code = " << e.getErrorCode() << std::endl;
    std::cout << e.getMessage() << std::endl;
    return MappingResult(false, empty_mapping, ElapsedSeconds(start_time));
  }
}

void mapper::ConnectivityBasedILPMapper::SetLogFilePath(
    const std::string& log_file_path) {
  log_file_path_ = log_file_path;
}

void mapper::ConnectivityBasedILPMapper::SetTimeOut(double timeout_s) {
  timeout_s_ = timeout_s;
}

void mapper::ConnectivityBasedILPMapper::SetAcceptFeasibleSolution(
    bool accept_feasible_solution) {
  accept_feasible_solution_ = accept_feasible_solution;
}
