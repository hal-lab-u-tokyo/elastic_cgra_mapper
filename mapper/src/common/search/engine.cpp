#include "engine_internal.hpp"

namespace mapper::detail::placement_search {

double SecondsSince(std::chrono::steady_clock::time_point start) {
  const auto now = std::chrono::steady_clock::now();
  return std::chrono::duration_cast<std::chrono::duration<double>>(now - start)
      .count();
}

bool SupportsOperation(const entity::MRRGNodeProperty& node_property,
                       entity::OpType op) {
  return std::find(node_property.supported_operations.begin(),
                   node_property.supported_operations.end(),
                   op) != node_property.supported_operations.end();
}

int DirectedDistanceCost(int distance) {
  return distance >= kInfDistance ? kInfDistance : std::max(1, distance);
}

std::vector<DFGEdgeInfo> BuildDFGEdges(entity::DFG& dfg) {
  std::vector<DFGEdgeInfo> edges;
  int edge_id = 0;
  for (int source = 0; source < dfg.GetNodeNum(); source++) {
    for (int target : dfg.GetAdjacentNodeIdVec(source)) {
      edges.push_back({edge_id++, source, target});
    }
  }
  return edges;
}

std::vector<std::vector<int>> BuildSuccessors(entity::DFG& dfg) {
  std::vector<std::vector<int>> successors(dfg.GetNodeNum());
  for (int i = 0; i < dfg.GetNodeNum(); i++) {
    successors[i] = dfg.GetAdjacentNodeIdVec(i);
  }
  return successors;
}

std::vector<std::vector<int>> BuildPredecessors(entity::DFG& dfg) {
  std::vector<std::vector<int>> predecessors(dfg.GetNodeNum());
  for (int i = 0; i < dfg.GetNodeNum(); i++) {
    for (int successor : dfg.GetAdjacentNodeIdVec(i)) {
      if (successor == i) continue;
      predecessors[successor].push_back(i);
    }
  }
  return predecessors;
}

std::vector<std::vector<int>> BuildIncidentEdgeIds(
    int node_num, const std::vector<DFGEdgeInfo>& edges) {
  std::vector<std::vector<int>> incident_edge_ids(node_num);
  for (const auto& edge : edges) {
    if (edge.source >= 0 && edge.source < node_num) {
      incident_edge_ids[edge.source].push_back(edge.id);
    }
    if (edge.target >= 0 && edge.target < node_num &&
        edge.target != edge.source) {
      incident_edge_ids[edge.target].push_back(edge.id);
    }
  }
  return incident_edge_ids;
}

PlacementSearchEngine::PlacementSearchEngine(
    entity::DFG& dfg, entity::MRRG& mrrg, PlacementSearchKind search_kind,
    double timeout_s, const std::optional<std::string>& log_file_path,
    const PlacementSearchOptions& options)
    : dfg_(dfg),
      mrrg_(mrrg),
      search_kind_(search_kind),
      timeout_s_(timeout_s > 0.0 ? timeout_s : 1.0),
      log_file_path_(log_file_path),
      options_(options),
      base_seed_(static_cast<unsigned int>(
          0x9E3779B9u ^ static_cast<unsigned int>(dfg.GetNodeNum() * 131 +
                                                  mrrg.GetNodeNum()))),
      rng_(base_seed_) {
  ResetSeed(0);
  dfg_edges_ = BuildDFGEdges(dfg_);
  incident_edge_ids_ = BuildIncidentEdgeIds(dfg_.GetNodeNum(), dfg_edges_);
  successors_ = BuildSuccessors(dfg_);
  predecessors_ = BuildPredecessors(dfg_);
  BuildMRRGCache();
  if (NeedsAllPairsDistances()) {
    BuildAllPairsDistances();
  }
  if (IsPRISALike()) {
    BuildPRISADistanceRegions();
  }
  annotation_ = BuildAnnotation();
}

mapper::MappingResult PlacementSearchEngine::Run() {
  const auto start = std::chrono::steady_clock::now();
  Log("start mapper=" + MapperName() +
      " dfg_nodes=" + std::to_string(dfg_.GetNodeNum()) +
      " mrrg_nodes=" + std::to_string(mrrg_.GetNodeNum()) +
      " max_trials=" + std::to_string(MaxTrials()) +
      " seed_count=" + std::to_string(SeedCount()) + " routing_retry_count=" +
      std::to_string(RoutingRetryCount()) + " max_iterations=" +
      std::to_string(IsPRISALike() ? PRISAMaxIterations() : MaxIterations()));

  std::optional<PlacementState> placement;
  if (IsPRISALike()) {
    placement = RunPRISAMultiSeed(start);
  } else if (IsSALike()) {
    placement = RunSAMultiSeed(start);
  } else {
    placement = RunGreedyMultiStart(start);
  }

  if (!placement.has_value()) {
    return MakeFailureResult(start);
  }

  if (options_.placement_only) {
    const auto mapping = MakePlacementOnlyMapping(*placement);
    const double mapping_time_s = SecondsSince(start);
    Log("placement_only_success mapper=" + MapperName() +
        " time_s=" + std::to_string(mapping_time_s) +
        " placement_cost=" + std::to_string(PlacementCost(*placement)));
    return mapper::MappingResult(true, mapping, mapping_time_s);
  }

  std::optional<RouteUsage> route_usage = TryRoutePlacement(*placement);
  if (!route_usage.has_value()) {
    return MakeFailureResult(start);
  }

  for (auto& edges : route_usage->source_to_route_edges) {
    std::sort(edges.begin(), edges.end());
    edges.erase(std::unique(edges.begin(), edges.end()), edges.end());
  }

  const auto mapping = entity::GenerateMappingFromRoutingResult(
      mrrg_, dfg_, placement->dfg_to_mrrg, route_usage->source_to_route_edges);
  const double mapping_time_s = SecondsSince(start);
  Log("success mapper=" + MapperName() +
      " time_s=" + std::to_string(mapping_time_s) +
      " placement_cost=" + std::to_string(PlacementCost(*placement)) +
      " route_edges=" + std::to_string(RouteEdgeCount(*route_usage)));
  return mapper::MappingResult(true, mapping, mapping_time_s);
}

}  // namespace mapper::detail::placement_search

namespace mapper::detail {

MappingResult RunPlacementSearchMapper(
    const std::shared_ptr<entity::DFG>& dfg_ptr,
    const std::shared_ptr<entity::MRRG>& mrrg_ptr,
    PlacementSearchKind search_kind, const std::optional<double>& timeout_s,
    const std::optional<std::string>& log_file_path,
    const PlacementSearchOptions& options) {
  const double effective_timeout =
      timeout_s.has_value() ? timeout_s.value() : 1.0;
  placement_search::PlacementSearchEngine engine(*dfg_ptr, *mrrg_ptr,
                                                 search_kind, effective_timeout,
                                                 log_file_path, options);
  return engine.Run();
}

}  // namespace mapper::detail
