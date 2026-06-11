#include <gurobi_c++.h>

#include <cassert>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <memory>
#include <vector>
#include <mapper/mapper_factory.hpp>
#include <mapper/gurobi_ilp_mapper.hpp>

namespace {
const bool kGurobiILPMapperRegistered =
    mapper::RegisterMapperType<mapper::GurobiILPMapper>("ILPMapper");

struct DFGEdge {
  int source;
  int target;
};

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
}

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
  try {
    // create gurobi env
    GRBEnv env = GRBEnv(true);
    // Let Gurobi choose the thread count; fixed high values are fragile in
    // container environments.
    env.start();

    // create an empty model
    GRBModel model = GRBModel(env);
    if (log_file_path_.has_value()) {
      model.set("LogFile", log_file_path_.value());
    }
    if (timeout_s_.has_value()) {
      model.set(GRB_DoubleParam_TimeLimit, timeout_s_.value());
    }

    int dfg_node_num = dfg_ptr_->GetNodeNum();
    int mrrg_node_num = mrrg_ptr_->GetNodeNum();
    int mrrg_edge_num = mrrg_ptr_->GetEdgeNum();
    std::vector<DFGEdge> dfg_edge_vec = BuildDFGEdgeVec(*dfg_ptr_);
    int dfg_edge_num = static_cast<int>(dfg_edge_vec.size());

    // create variable
    std::vector<std::vector<GRBVar>> map_op_to_PE(
        dfg_node_num, std::vector<GRBVar>(mrrg_node_num));
    std::vector<std::vector<GRBVar>> map_source_to_route_edge(
        dfg_node_num, std::vector<GRBVar>(mrrg_edge_num));
    std::vector<std::vector<GRBVar>> map_source_to_route_PE(
        dfg_node_num, std::vector<GRBVar>(mrrg_node_num));
    std::vector<std::vector<GRBVar>> map_dfg_edge_to_mrrg_edge(
        dfg_edge_num, std::vector<GRBVar>(mrrg_edge_num));
    std::vector<std::vector<GRBVar>> map_dfg_edge_to_route_PE(
        dfg_edge_num, std::vector<GRBVar>(mrrg_node_num));

    for (int dfg_node_id = 0; dfg_node_id < dfg_node_num; dfg_node_id++) {
      for (int mrrg_node_id = 0; mrrg_node_id < mrrg_node_num; mrrg_node_id++) {
        std::string var_name;
        var_name = "f_" + std::to_string(dfg_node_id) + "_" +
                   std::to_string(mrrg_node_id);
        map_op_to_PE[dfg_node_id][mrrg_node_id] =
            model.addVar(0.0, 1.0, 0.0, GRB_BINARY, var_name);
        var_name = "source_route_node_" + std::to_string(dfg_node_id) + "_" +
                   std::to_string(mrrg_node_id);
        map_source_to_route_PE[dfg_node_id][mrrg_node_id] =
            model.addVar(0.0, 1.0, 0.0, GRB_BINARY, var_name);
      }
      for (int mrrg_edge_id = 0; mrrg_edge_id < mrrg_edge_num; mrrg_edge_id++) {
        std::string var_name;
        var_name = "source_route_edge_" + std::to_string(dfg_node_id) + "_" +
                   std::to_string(mrrg_edge_id);
        map_source_to_route_edge[dfg_node_id][mrrg_edge_id] =
            model.addVar(0.0, 1.0, 0.0, GRB_BINARY, var_name);
      }
    }
    for (int dfg_edge_id = 0; dfg_edge_id < dfg_edge_num; dfg_edge_id++) {
      for (int mrrg_edge_id = 0; mrrg_edge_id < mrrg_edge_num; mrrg_edge_id++) {
        std::string var_name = "dfg_edge_route_" +
                               std::to_string(dfg_edge_id) + "_" +
                               std::to_string(mrrg_edge_id);
        map_dfg_edge_to_mrrg_edge[dfg_edge_id][mrrg_edge_id] =
            model.addVar(0.0, 1.0, 0.0, GRB_BINARY, var_name);
      }
      for (int mrrg_node_id = 0; mrrg_node_id < mrrg_node_num; mrrg_node_id++) {
        std::string var_name = "dfg_edge_route_node_" +
                               std::to_string(dfg_edge_id) + "_" +
                               std::to_string(mrrg_node_id);
        map_dfg_edge_to_route_PE[dfg_edge_id][mrrg_node_id] =
            model.addVar(0.0, 1.0, 0.0, GRB_BINARY, var_name);
      }
    }

    // set objective
    GRBLinExpr objective_lin_expr;
    const double object_coefficient = 1.0;
    for (int mrrg_edge_id = 0; mrrg_edge_id < mrrg_edge_num; mrrg_edge_id++) {
      for (int dfg_node_id = 0; dfg_node_id < dfg_node_num; dfg_node_id++) {
        objective_lin_expr.addTerms(&object_coefficient,
                                    &(map_source_to_route_edge[dfg_node_id]
                                                              [mrrg_edge_id]),
                                    1);
      }
    }
    model.setObjective(objective_lin_expr, GRB_MINIMIZE);

    // add constraint
    // add constraint: operation placement
    const double placement_coefficient = 1;
    for (int dfg_node_id = 0; dfg_node_id < dfg_node_num; dfg_node_id++) {
      GRBLinExpr tmp_lin_expr;
      for (int mrrg_node_id = 0; mrrg_node_id < mrrg_node_num; mrrg_node_id++) {
        tmp_lin_expr.addTerms(&placement_coefficient,
                              &(map_op_to_PE[dfg_node_id][mrrg_node_id]), 1);
      }
      std::string constr_name = "c_placement_" + std::to_string(dfg_node_id);
      model.addConstr(tmp_lin_expr, GRB_EQUAL, 1, constr_name);
    }

    // add constraint: functional unit exclusivity
    const double exclusivity_coefficient = 1;
    for (int mrrg_node_id = 0; mrrg_node_id < mrrg_node_num; mrrg_node_id++) {
      GRBLinExpr tmp_lin_expr;
      for (int dfg_node_id = 0; dfg_node_id < dfg_node_num; dfg_node_id++) {
        tmp_lin_expr.addTerms(&exclusivity_coefficient,
                              &(map_op_to_PE[dfg_node_id][mrrg_node_id]), 1);
        tmp_lin_expr.addTerms(&exclusivity_coefficient,
                              &(map_source_to_route_PE[dfg_node_id]
                                                       [mrrg_node_id]),
                              1);
      }
      std::string constr_name = "c_exclusivity_" + std::to_string(mrrg_node_id);
      model.addConstr(tmp_lin_expr, GRB_LESS_EQUAL, 1, constr_name);
    }

    // add constraint: functional unit legality
    for (int dfg_node_id = 0; dfg_node_id < dfg_node_num; dfg_node_id++) {
      for (int mrrg_node_id = 0; mrrg_node_id < mrrg_node_num; mrrg_node_id++) {
        entity::OpType dfg_op = (dfg_ptr_->GetNodeProperty(dfg_node_id)).op;
        std::vector<entity::OpType> mrrg_supported_op_vec =
            (mrrg_ptr_->GetNodeProperty(mrrg_node_id)).supported_operations;
        bool support_op = false;
        for (entity::OpType supported_op : mrrg_supported_op_vec) {
          if (dfg_op == supported_op) support_op = true;
        }

        if (!support_op) {
          std::string constr_name = "c_legality_" +
                                    std::to_string(dfg_node_id) + "_" +
                                    std::to_string(mrrg_node_id);
          model.addConstr(map_op_to_PE[dfg_node_id][mrrg_node_id], GRB_EQUAL, 0,
                          constr_name);
        }
      }
    }

    // add constraint: exact per-DFG-edge routing flow
    const double flow_coefficient = 1.0;
    for (int dfg_edge_id = 0; dfg_edge_id < dfg_edge_num; dfg_edge_id++) {
      int source_dfg_node_id = dfg_edge_vec[dfg_edge_id].source;
      int target_dfg_node_id = dfg_edge_vec[dfg_edge_id].target;
      for (int mrrg_node_id = 0; mrrg_node_id < mrrg_node_num; mrrg_node_id++) {
        std::vector<int> mrrg_out_edge_ids =
            mrrg_ptr_->GetOutEdgeIdVec(mrrg_node_id);
        std::vector<int> mrrg_in_edge_ids =
            mrrg_ptr_->GetInEdgeIdVec(mrrg_node_id);
        GRBLinExpr out_flow;
        GRBLinExpr in_flow;
        for (int mrrg_out_edge_id : mrrg_out_edge_ids) {
          out_flow.addTerms(
              &flow_coefficient,
              &(map_dfg_edge_to_mrrg_edge[dfg_edge_id][mrrg_out_edge_id]), 1);
        }
        for (int mrrg_in_edge_id : mrrg_in_edge_ids) {
          in_flow.addTerms(
              &flow_coefficient,
              &(map_dfg_edge_to_mrrg_edge[dfg_edge_id][mrrg_in_edge_id]), 1);
        }

        // Each DFG edge is routed as a single commodity from the placed source
        // operation to the placed target operation. This is stricter than the
        // previous producer-level constraints and prevents fanout sinks from
        // being reported as mapped without a real MRRG path.
        std::string constr_name =
            "c_edge_flow_" + std::to_string(dfg_edge_id) + "_" +
            std::to_string(mrrg_node_id);
        model.addConstr(
            out_flow - in_flow, GRB_EQUAL,
            map_op_to_PE[source_dfg_node_id][mrrg_node_id] -
                map_op_to_PE[target_dfg_node_id][mrrg_node_id],
            constr_name);

        model.addConstr(out_flow, GRB_LESS_EQUAL, 1,
                        constr_name + "_single_out");
        model.addConstr(in_flow, GRB_LESS_EQUAL, 1,
                        constr_name + "_single_in");
        model.addConstr(map_dfg_edge_to_route_PE[dfg_edge_id][mrrg_node_id],
                        GRB_LESS_EQUAL, in_flow, constr_name + "_route_in");
        model.addConstr(map_dfg_edge_to_route_PE[dfg_edge_id][mrrg_node_id],
                        GRB_LESS_EQUAL, out_flow, constr_name + "_route_out");
        model.addConstr(
            map_dfg_edge_to_route_PE[dfg_edge_id][mrrg_node_id],
            GRB_LESS_EQUAL,
            1 - map_op_to_PE[source_dfg_node_id][mrrg_node_id],
            constr_name + "_route_not_source_op");
        model.addConstr(
            map_dfg_edge_to_route_PE[dfg_edge_id][mrrg_node_id],
            GRB_LESS_EQUAL,
            1 - map_op_to_PE[target_dfg_node_id][mrrg_node_id],
            constr_name + "_route_not_target_op");
        model.addConstr(
            map_dfg_edge_to_route_PE[dfg_edge_id][mrrg_node_id],
            GRB_GREATER_EQUAL,
            in_flow + out_flow - 1 -
                map_op_to_PE[source_dfg_node_id][mrrg_node_id] -
                map_op_to_PE[target_dfg_node_id][mrrg_node_id],
            constr_name + "_route_internal");

        if (source_dfg_node_id == target_dfg_node_id) {
          // A loop-carried self-edge has the same DFG node as source and sink,
          // so ordinary out-in flow balance has a zero RHS everywhere. Force a
          // real MRRG cycle through the placed operation instead of accepting a
          // zero-length route.
          model.addConstr(out_flow, GRB_GREATER_EQUAL,
                          map_op_to_PE[source_dfg_node_id][mrrg_node_id],
                          constr_name + "_self_loop_out");
          model.addConstr(in_flow, GRB_GREATER_EQUAL,
                          map_op_to_PE[source_dfg_node_id][mrrg_node_id],
                          constr_name + "_self_loop_in");
        }
      }
    }

    // add constraint: producer-level physical route ownership
    for (int dfg_node_id = 0; dfg_node_id < dfg_node_num; dfg_node_id++) {
      for (int mrrg_edge_id = 0; mrrg_edge_id < mrrg_edge_num; mrrg_edge_id++) {
        GRBLinExpr routes_from_source;
        int source_edge_count = 0;
        for (int dfg_edge_id = 0; dfg_edge_id < dfg_edge_num; dfg_edge_id++) {
          if (dfg_edge_vec[dfg_edge_id].source != dfg_node_id) {
            continue;
          }
          source_edge_count++;
          routes_from_source.addTerms(
              &flow_coefficient,
              &(map_dfg_edge_to_mrrg_edge[dfg_edge_id][mrrg_edge_id]), 1);
          model.addConstr(
              map_dfg_edge_to_mrrg_edge[dfg_edge_id][mrrg_edge_id],
              GRB_LESS_EQUAL, map_source_to_route_edge[dfg_node_id][mrrg_edge_id],
              "c_source_route_edge_lb_" + std::to_string(dfg_node_id) + "_" +
                  std::to_string(dfg_edge_id) + "_" +
                  std::to_string(mrrg_edge_id));
        }
        std::string constr_name =
            "c_source_route_edge_ub_" + std::to_string(dfg_node_id) + "_" +
            std::to_string(mrrg_edge_id);
        if (source_edge_count > 0) {
          model.addConstr(map_source_to_route_edge[dfg_node_id][mrrg_edge_id],
                          GRB_LESS_EQUAL, routes_from_source, constr_name);
        } else {
          model.addConstr(map_source_to_route_edge[dfg_node_id][mrrg_edge_id],
                          GRB_EQUAL, 0, constr_name);
        }
      }

      for (int mrrg_node_id = 0; mrrg_node_id < mrrg_node_num; mrrg_node_id++) {
        GRBLinExpr route_nodes_from_source;
        int source_edge_count = 0;
        for (int dfg_edge_id = 0; dfg_edge_id < dfg_edge_num; dfg_edge_id++) {
          if (dfg_edge_vec[dfg_edge_id].source != dfg_node_id) {
            continue;
          }
          source_edge_count++;
          route_nodes_from_source.addTerms(
              &flow_coefficient,
              &(map_dfg_edge_to_route_PE[dfg_edge_id][mrrg_node_id]), 1);
          model.addConstr(
              map_dfg_edge_to_route_PE[dfg_edge_id][mrrg_node_id],
              GRB_LESS_EQUAL, map_source_to_route_PE[dfg_node_id][mrrg_node_id],
              "c_source_route_node_lb_" + std::to_string(dfg_node_id) + "_" +
                  std::to_string(dfg_edge_id) + "_" +
                  std::to_string(mrrg_node_id));
        }
        std::string constr_name =
            "c_source_route_node_ub_" + std::to_string(dfg_node_id) + "_" +
            std::to_string(mrrg_node_id);
        if (source_edge_count > 0) {
          model.addConstr(map_source_to_route_PE[dfg_node_id][mrrg_node_id],
                          GRB_LESS_EQUAL, route_nodes_from_source, constr_name);
        } else {
          model.addConstr(map_source_to_route_PE[dfg_node_id][mrrg_node_id],
                          GRB_EQUAL, 0, constr_name);
        }
      }
    }

    // add constraint: route resource exclusivity and route-node legality
    for (int mrrg_edge_id = 0; mrrg_edge_id < mrrg_edge_num; mrrg_edge_id++) {
      GRBLinExpr edge_owner_num;
      for (int dfg_node_id = 0; dfg_node_id < dfg_node_num; dfg_node_id++) {
        edge_owner_num.addTerms(
            &flow_coefficient,
            &(map_source_to_route_edge[dfg_node_id][mrrg_edge_id]), 1);
      }
      std::string constr_name =
          "c_route_edge_exclusivity_" + std::to_string(mrrg_edge_id);
      model.addConstr(edge_owner_num, GRB_LESS_EQUAL, 1, constr_name);
    }

    for (int mrrg_node_id = 0; mrrg_node_id < mrrg_node_num; mrrg_node_id++) {
      if (SupportsOperation(mrrg_ptr_->GetNodeProperty(mrrg_node_id),
                            entity::OpType::ROUTE)) {
        continue;
      }
      for (int dfg_node_id = 0; dfg_node_id < dfg_node_num; dfg_node_id++) {
        std::string constr_name =
            "c_route_node_legality_" + std::to_string(dfg_node_id) + "_" +
            std::to_string(mrrg_node_id);
        model.addConstr(map_source_to_route_PE[dfg_node_id][mrrg_node_id],
                        GRB_EQUAL, 0, constr_name);
      }
    }

    // add constraint for elastic CGRA
    if (mrrg_ptr_->GetMRRGConfig().cgra_type ==
        entity::MRRGCGRAType::kElastic) {
      // TODO(msBRF65): implement constraints for elastic CGRA
    }

    // optimize model
    model.optimize();

    int status = model.get(GRB_IntAttr_Status);

    // Time-limited runs may still have a feasible incumbent. Keep that
    // behavior configurable for experiments that require stricter status.
    if (status == GRB_INFEASIBLE) {
      model.computeIIS();
      model.write("debug.iis");
      const auto end_time = std::chrono::system_clock::now();
      const double mapping_time =
          std::chrono::duration_cast<std::chrono::milliseconds>(end_time -
                                                                start_time)
              .count() /
          1000.0;
      return MappingResult(false, entity::Mapping(mrrg_ptr_->GetMRRGConfig()),
                           mapping_time);
    }

    if (!accept_feasible_solution_ && status != GRB_OPTIMAL &&
        status != GRB_SUBOPTIMAL) {
      const auto end_time = std::chrono::system_clock::now();
      const double mapping_time =
          std::chrono::duration_cast<std::chrono::milliseconds>(end_time -
                                                                start_time)
              .count() /
          1000.0;
      return MappingResult(false, entity::Mapping(mrrg_ptr_->GetMRRGConfig()),
                           mapping_time);
    }

    if (model.get(GRB_IntAttr_SolCount) == 0) {
      const auto end_time = std::chrono::system_clock::now();
      const double mapping_time =
          std::chrono::duration_cast<std::chrono::milliseconds>(end_time -
                                                                start_time)
              .count() /
          1000.0;
      return MappingResult(false, entity::Mapping(mrrg_ptr_->GetMRRGConfig()),
                           mapping_time);
    }

    // get result
    std::vector<int> dfg_node_to_mrrg_node(dfg_node_num);
    std::vector<std::vector<int>> dfg_output_to_mrrg_edge(dfg_node_num);

    for (int i = 0; i < dfg_node_num; i++) {
      // Binary solution values are returned as doubles; read them in bulk and
      // use a 0.5 threshold instead of exact equality to tolerate solver eps.
      std::unique_ptr<double[]> map_op_to_PE_values(
          model.get(GRB_DoubleAttr_X, map_op_to_PE[i].data(), mrrg_node_num));
      for (int j = 0; j < mrrg_node_num; j++) {
        if (map_op_to_PE_values[j] > 0.5) {
          dfg_node_to_mrrg_node[i] = j;
        }
      }
    }
    for (int dfg_edge_id = 0; dfg_edge_id < dfg_edge_num; dfg_edge_id++) {
      int source_dfg_node_id = dfg_edge_vec[dfg_edge_id].source;
      std::unique_ptr<double[]> route_edge_values(
          model.get(GRB_DoubleAttr_X,
                    map_dfg_edge_to_mrrg_edge[dfg_edge_id].data(),
                    mrrg_edge_num));
      for (int mrrg_edge_id = 0; mrrg_edge_id < mrrg_edge_num; mrrg_edge_id++) {
        if (route_edge_values[mrrg_edge_id] > 0.5 &&
            std::find(dfg_output_to_mrrg_edge[source_dfg_node_id].begin(),
                      dfg_output_to_mrrg_edge[source_dfg_node_id].end(),
                      mrrg_edge_id) ==
                dfg_output_to_mrrg_edge[source_dfg_node_id].end()) {
          dfg_output_to_mrrg_edge[source_dfg_node_id].push_back(mrrg_edge_id);
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

    const auto end_time = std::chrono::system_clock::now();
    const double mapping_time =
        std::chrono::duration_cast<std::chrono::milliseconds>(end_time -
                                                              start_time)
            .count() /
        1000.0;

    return MappingResult(false, entity::Mapping(mrrg_ptr_->GetMRRGConfig()),
                         mapping_time);
  }
}

void mapper::GurobiILPMapper::SetLogFilePath(const std::string& log_file_path) {
  log_file_path_ = log_file_path;
  return;
}

void mapper::GurobiILPMapper::SetTimeOut(double timeout_s) {
  timeout_s_ = timeout_s;
  return;
}

void mapper::GurobiILPMapper::SetAcceptFeasibleSolution(
    bool accept_feasible_solution) {
  accept_feasible_solution_ = accept_feasible_solution;
  return;
}
