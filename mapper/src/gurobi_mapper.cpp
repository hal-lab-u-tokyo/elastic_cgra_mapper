#include <gurobi_c++.h>

#include <algorithm>
#include <chrono>
#include <iostream>
#include <mapper/gurobi_mapper.hpp>
#include <string>
#include <vector>

namespace {

struct DFGEdgeInfo {
  int src;
  int dst;
};

bool SupportsOp(const entity::MRRGNodeProperty& node_property,
                entity::OpType op) {
  return std::find(node_property.supported_operations.begin(),
                   node_property.supported_operations.end(),
                   op) != node_property.supported_operations.end();
}

std::vector<DFGEdgeInfo> CollectDFGEdges(entity::DFG& dfg) {
  std::vector<DFGEdgeInfo> result;
  for (int edge_id = 0; edge_id < dfg.GetEdgeNum(); ++edge_id) {
    const auto src_and_dst = dfg.GetEdgeSourceTarget(edge_id);
    result.push_back({src_and_dst.first, src_and_dst.second});
  }
  return result;
}

std::vector<entity::Edge> CollectMRRGEdges(entity::MRRG& mrrg) {
  std::vector<entity::Edge> result;
  for (int edge_id = 0; edge_id < mrrg.GetEdgeNum(); ++edge_id) {
    result.push_back(mrrg.GetEdgeSourceTarget(edge_id));
  }
  return result;
}

std::vector<std::vector<int>> CollectDFGOutgoingEdges(
    const std::vector<DFGEdgeInfo>& dfg_edges, int dfg_node_num) {
  std::vector<std::vector<int>> result(dfg_node_num);
  for (int edge_id = 0; edge_id < static_cast<int>(dfg_edges.size());
       ++edge_id) {
    result[dfg_edges[edge_id].src].push_back(edge_id);
  }
  return result;
}

std::vector<std::vector<int>> CollectMRRGIncidentEdges(
    const std::vector<entity::Edge>& mrrg_edges, int mrrg_node_num) {
  std::vector<std::vector<int>> result(mrrg_node_num);
  for (int edge_id = 0; edge_id < static_cast<int>(mrrg_edges.size());
       ++edge_id) {
    result[mrrg_edges[edge_id].first].push_back(edge_id);
    result[mrrg_edges[edge_id].second].push_back(edge_id);
  }
  return result;
}

}  // namespace

mapper::GurobiILPMapper::GurobiILPMapper(
    const std::shared_ptr<entity::DFG> dfg_ptr,
    const std::shared_ptr<entity::MRRG> mrrg_ptr)
    : dfg_ptr_(dfg_ptr), mrrg_ptr_(mrrg_ptr) {}

mapper::GurobiILPMapper* mapper::GurobiILPMapper::CreateMapper(
    const std::shared_ptr<entity::DFG> dfg_ptr,
    const std::shared_ptr<entity::MRRG> mrrg_ptr) {
  mapper::GurobiILPMapper* result = new mapper::GurobiILPMapper;
  *result = mapper::GurobiILPMapper(dfg_ptr, mrrg_ptr);

  return result;
}

mapper::MappingResult mapper::GurobiILPMapper::Execution() {
  const auto start_time = std::chrono::system_clock::now();

  auto MakeFailure = [&]() {
    const auto end_time = std::chrono::system_clock::now();
    const double mapping_time =
        std::chrono::duration_cast<std::chrono::milliseconds>(end_time -
                                                              start_time)
            .count() /
        1000.0;
    return MappingResult(false, entity::Mapping(mrrg_ptr_->GetMRRGConfig()),
                         mapping_time);
  };

  try {
    GRBEnv env = GRBEnv(true);
    env.set(GRB_IntParam_Threads, 32);
    // env.set(GRB_IntParam_SolutionLimit, 3);
    env.set(GRB_IntParam_MIPFocus, 1);
    env.start();

    GRBModel model = GRBModel(env);
    if (log_file_path_.has_value()) {
      model.set("LogFile", log_file_path_.value());
    }
    if (timeout_s_.has_value()) {
      model.set(GRB_DoubleParam_TimeLimit, timeout_s_.value());
    }

    const int dfg_node_num = dfg_ptr_->GetNodeNum();
    const int mrrg_node_num = mrrg_ptr_->GetNodeNum();
    const int mrrg_edge_num = mrrg_ptr_->GetEdgeNum();
    const std::vector<DFGEdgeInfo> dfg_edges = CollectDFGEdges(*dfg_ptr_);
    const int dfg_edge_num = static_cast<int>(dfg_edges.size());
    const std::vector<entity::Edge> mrrg_edges = CollectMRRGEdges(*mrrg_ptr_);
    const auto dfg_out_edges =
        CollectDFGOutgoingEdges(dfg_edges, dfg_node_num);
    const auto mrrg_incident_edges =
        CollectMRRGIncidentEdges(mrrg_edges, mrrg_node_num);

    // F[o][n]: DFG op o is placed on MRRG PE/context n.
    std::vector<std::vector<GRBVar>> place_op(
        dfg_node_num, std::vector<GRBVar>(mrrg_node_num));

    // R[u][e]: the output value of DFG op u uses MRRG edge e.
    std::vector<std::vector<GRBVar>> value_uses_edge(
        dfg_node_num, std::vector<GRBVar>(mrrg_edge_num));

    // S[q][e]: DFG edge q=(u,v), i.e. one fanout of u, uses MRRG edge e.
    std::vector<std::vector<GRBVar>> fanout_uses_edge(
        dfg_edge_num, std::vector<GRBVar>(mrrg_edge_num));

    // U[u][n]: the output value of u occupies n as an intermediate route node.
    std::vector<std::vector<GRBVar>> value_uses_route_node(
        dfg_node_num, std::vector<GRBVar>(mrrg_node_num));

    // W[q][n]: fanout q occupies n as an intermediate route node.
    std::vector<std::vector<GRBVar>> fanout_uses_route_node(
        dfg_edge_num, std::vector<GRBVar>(mrrg_node_num));

    for (int dfg_node_id = 0; dfg_node_id < dfg_node_num; ++dfg_node_id) {
      for (int mrrg_node_id = 0; mrrg_node_id < mrrg_node_num;
           ++mrrg_node_id) {
        place_op[dfg_node_id][mrrg_node_id] = model.addVar(
            0.0, 1.0, 0.0, GRB_BINARY,
            "F_" + std::to_string(dfg_node_id) + "_" +
                std::to_string(mrrg_node_id));
        value_uses_route_node[dfg_node_id][mrrg_node_id] = model.addVar(
            0.0, 1.0, 0.0, GRB_BINARY,
            "U_" + std::to_string(dfg_node_id) + "_" +
                std::to_string(mrrg_node_id));
      }
      for (int mrrg_edge_id = 0; mrrg_edge_id < mrrg_edge_num;
           ++mrrg_edge_id) {
        value_uses_edge[dfg_node_id][mrrg_edge_id] = model.addVar(
            0.0, 1.0, 0.0, GRB_BINARY,
            "R_" + std::to_string(dfg_node_id) + "_" +
                std::to_string(mrrg_edge_id));
      }
    }

    for (int dfg_edge_id = 0; dfg_edge_id < dfg_edge_num; ++dfg_edge_id) {
      for (int mrrg_edge_id = 0; mrrg_edge_id < mrrg_edge_num;
           ++mrrg_edge_id) {
        fanout_uses_edge[dfg_edge_id][mrrg_edge_id] = model.addVar(
            0.0, 1.0, 0.0, GRB_BINARY,
            "S_" + std::to_string(dfg_edge_id) + "_" +
                std::to_string(mrrg_edge_id));
      }
      for (int mrrg_node_id = 0; mrrg_node_id < mrrg_node_num;
           ++mrrg_node_id) {
        fanout_uses_route_node[dfg_edge_id][mrrg_node_id] = model.addVar(
            0.0, 1.0, 0.0, GRB_BINARY,
            "W_" + std::to_string(dfg_edge_id) + "_" +
                std::to_string(mrrg_node_id));
      }
    }

    GRBLinExpr objective;
    for (int dfg_node_id = 0; dfg_node_id < dfg_node_num; ++dfg_node_id) {
      for (int mrrg_edge_id = 0; mrrg_edge_id < mrrg_edge_num;
           ++mrrg_edge_id) {
        objective += value_uses_edge[dfg_node_id][mrrg_edge_id];
      }
    }
    model.setObjective(objective, GRB_MINIMIZE);

    // Each DFG op is placed exactly once.
    for (int dfg_node_id = 0; dfg_node_id < dfg_node_num; ++dfg_node_id) {
      GRBLinExpr placement_sum;
      for (int mrrg_node_id = 0; mrrg_node_id < mrrg_node_num;
           ++mrrg_node_id) {
        placement_sum += place_op[dfg_node_id][mrrg_node_id];
      }
      model.addConstr(placement_sum == 1,
                      "c_placement_" + std::to_string(dfg_node_id));
    }

    // A DFG op can only be placed on a node that supports its operation.
    for (int dfg_node_id = 0; dfg_node_id < dfg_node_num; ++dfg_node_id) {
      const entity::OpType dfg_op = dfg_ptr_->GetNodeProperty(dfg_node_id).op;
      for (int mrrg_node_id = 0; mrrg_node_id < mrrg_node_num;
           ++mrrg_node_id) {
        if (!SupportsOp(mrrg_ptr_->GetNodeProperty(mrrg_node_id), dfg_op)) {
          model.addConstr(place_op[dfg_node_id][mrrg_node_id] == 0,
                          "c_legality_" + std::to_string(dfg_node_id) + "_" +
                              std::to_string(mrrg_node_id));
        }
      }
    }

    // Fanout-specific S variables are sub-uses of the source value's R tree.
    for (int dfg_edge_id = 0; dfg_edge_id < dfg_edge_num; ++dfg_edge_id) {
      const int src_dfg_node_id = dfg_edges[dfg_edge_id].src;
      for (int mrrg_edge_id = 0; mrrg_edge_id < mrrg_edge_num;
           ++mrrg_edge_id) {
        model.addConstr(
            fanout_uses_edge[dfg_edge_id][mrrg_edge_id] <=
                value_uses_edge[src_dfg_node_id][mrrg_edge_id],
            "c_sub_value_" + std::to_string(dfg_edge_id) + "_" +
                std::to_string(mrrg_edge_id));
      }
    }

    // Keep R equal to the union of all S variables for that value. This also
    // prevents dangling R edges for outputless DFG nodes.
    for (int dfg_node_id = 0; dfg_node_id < dfg_node_num; ++dfg_node_id) {
      for (int mrrg_edge_id = 0; mrrg_edge_id < mrrg_edge_num;
           ++mrrg_edge_id) {
        GRBLinExpr fanout_sum;
        for (int dfg_edge_id : dfg_out_edges[dfg_node_id]) {
          fanout_sum += fanout_uses_edge[dfg_edge_id][mrrg_edge_id];
        }
        model.addConstr(
            value_uses_edge[dfg_node_id][mrrg_edge_id] <= fanout_sum,
            "c_value_edge_union_" + std::to_string(dfg_node_id) + "_" +
                std::to_string(mrrg_edge_id));
      }
    }

    // For every DFG edge q=(src,dst), force a connected directed MRRG path
    // from the placed src node to the placed dst node.
    for (int dfg_edge_id = 0; dfg_edge_id < dfg_edge_num; ++dfg_edge_id) {
      const int src_dfg_node_id = dfg_edges[dfg_edge_id].src;
      const int dst_dfg_node_id = dfg_edges[dfg_edge_id].dst;
      for (int mrrg_node_id = 0; mrrg_node_id < mrrg_node_num;
           ++mrrg_node_id) {
        GRBLinExpr outgoing;
        for (int mrrg_edge_id : mrrg_ptr_->GetOutEdgeIdVec(mrrg_node_id)) {
          outgoing += fanout_uses_edge[dfg_edge_id][mrrg_edge_id];
        }

        GRBLinExpr incoming;
        for (int mrrg_edge_id : mrrg_ptr_->GetInEdgeIdVec(mrrg_node_id)) {
          incoming += fanout_uses_edge[dfg_edge_id][mrrg_edge_id];
        }

        model.addConstr(
            outgoing - incoming ==
                place_op[src_dfg_node_id][mrrg_node_id] -
                    place_op[dst_dfg_node_id][mrrg_node_id],
            "c_flow_" + std::to_string(dfg_edge_id) + "_" +
                std::to_string(mrrg_node_id));
      }
    }

    // Mark intermediate route-node use. Source and destination nodes of a
    // fanout are not route nodes for that fanout; every other touched node is.
    for (int dfg_edge_id = 0; dfg_edge_id < dfg_edge_num; ++dfg_edge_id) {
      const int src_dfg_node_id = dfg_edges[dfg_edge_id].src;
      const int dst_dfg_node_id = dfg_edges[dfg_edge_id].dst;
      for (int mrrg_node_id = 0; mrrg_node_id < mrrg_node_num;
           ++mrrg_node_id) {
        for (int mrrg_edge_id : mrrg_incident_edges[mrrg_node_id]) {
          model.addConstr(
              fanout_uses_route_node[dfg_edge_id][mrrg_node_id] >=
                  fanout_uses_edge[dfg_edge_id][mrrg_edge_id] -
                      place_op[src_dfg_node_id][mrrg_node_id] -
                      place_op[dst_dfg_node_id][mrrg_node_id],
              "c_fanout_route_node_" + std::to_string(dfg_edge_id) + "_" +
                  std::to_string(mrrg_node_id) + "_" +
                  std::to_string(mrrg_edge_id));
        }
        model.addConstr(
            value_uses_route_node[src_dfg_node_id][mrrg_node_id] >=
                fanout_uses_route_node[dfg_edge_id][mrrg_node_id],
            "c_value_route_node_" + std::to_string(dfg_edge_id) + "_" +
                std::to_string(mrrg_node_id));
      }
    }

    // A PE/context can host either one real DFG op or one routed value. Multiple
    // fanouts of the same value may share the same route node, matching CGRA-ME's
    // R/S split.
    for (int mrrg_node_id = 0; mrrg_node_id < mrrg_node_num; ++mrrg_node_id) {
      GRBLinExpr node_uses;
      for (int dfg_node_id = 0; dfg_node_id < dfg_node_num; ++dfg_node_id) {
        node_uses += place_op[dfg_node_id][mrrg_node_id];
        node_uses += value_uses_route_node[dfg_node_id][mrrg_node_id];
      }
      model.addConstr(node_uses <= 1,
                      "c_node_exclusivity_" + std::to_string(mrrg_node_id));
    }

    // An MRRG edge can be used by at most one DFG value. Fanouts of the same
    // value may share it through the single R variable.
    for (int mrrg_edge_id = 0; mrrg_edge_id < mrrg_edge_num; ++mrrg_edge_id) {
      GRBLinExpr edge_uses;
      for (int dfg_node_id = 0; dfg_node_id < dfg_node_num; ++dfg_node_id) {
        edge_uses += value_uses_edge[dfg_node_id][mrrg_edge_id];
      }
      model.addConstr(edge_uses <= 1,
                      "c_edge_exclusivity_" + std::to_string(mrrg_edge_id));
    }

    // Route nodes must be able to act as ROUTE operations.
    for (int dfg_node_id = 0; dfg_node_id < dfg_node_num; ++dfg_node_id) {
      for (int mrrg_node_id = 0; mrrg_node_id < mrrg_node_num;
           ++mrrg_node_id) {
        if (!SupportsOp(mrrg_ptr_->GetNodeProperty(mrrg_node_id),
                        entity::OpType::ROUTE)) {
          model.addConstr(value_uses_route_node[dfg_node_id][mrrg_node_id] ==
                              0,
                          "c_route_legality_" + std::to_string(dfg_node_id) +
                              "_" + std::to_string(mrrg_node_id));
        }
      }
    }

    if (mrrg_ptr_->GetMRRGConfig().cgra_type ==
        entity::MRRGCGRAType::kElastic) {
      // Elastic-specific ordering/timing constraints are still not represented
      // in this mapper. The core routing formulation above is connectivity-safe.
    }

    model.optimize();

    const int status = model.get(GRB_IntAttr_Status);
    if (status == GRB_INFEASIBLE) {
      model.computeIIS();
      model.write("debug.iis");
      return MakeFailure();
    }
    if (model.get(GRB_IntAttr_SolCount) == 0) {
      return MakeFailure();
    }
    if (status != GRB_OPTIMAL && status != GRB_SUBOPTIMAL &&
        status != GRB_TIME_LIMIT && status != GRB_SOLUTION_LIMIT) {
      return MakeFailure();
    }

    std::vector<int> dfg_node_to_mrrg_node(dfg_node_num, -1);
    std::vector<std::vector<int>> dfg_output_to_mrrg_edge(dfg_node_num);

    for (int dfg_node_id = 0; dfg_node_id < dfg_node_num; ++dfg_node_id) {
      for (int mrrg_node_id = 0; mrrg_node_id < mrrg_node_num;
           ++mrrg_node_id) {
        if (place_op[dfg_node_id][mrrg_node_id].get(GRB_DoubleAttr_X) > 0.5) {
          dfg_node_to_mrrg_node[dfg_node_id] = mrrg_node_id;
        }
      }
      for (int mrrg_edge_id = 0; mrrg_edge_id < mrrg_edge_num;
           ++mrrg_edge_id) {
        if (value_uses_edge[dfg_node_id][mrrg_edge_id].get(GRB_DoubleAttr_X) >
            0.5) {
          dfg_output_to_mrrg_edge[dfg_node_id].push_back(mrrg_edge_id);
        }
      }
    }

    const auto end_time = std::chrono::system_clock::now();
    const double mapping_time =
        std::chrono::duration_cast<std::chrono::milliseconds>(end_time -
                                                              start_time)
            .count() /
        1000.0;
    return MappingResult(true,
                         entity::GenerateMappingFromRoutingResult(
                             *mrrg_ptr_, *dfg_ptr_, dfg_node_to_mrrg_node,
                             dfg_output_to_mrrg_edge),
                         mapping_time);
  } catch (GRBException e) {
    std::cout << "Error code = " << e.getErrorCode() << std::endl;
    std::cout << e.getMessage() << std::endl;
    return MakeFailure();
  }
}

void mapper::GurobiILPMapper::SetLogFilePath(const std::string& log_file_path) {
  log_file_path_ = log_file_path;
}

void mapper::GurobiILPMapper::SetTimeOut(double timeout_s) {
  timeout_s_ = timeout_s;
}
