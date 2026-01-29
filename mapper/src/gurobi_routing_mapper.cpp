#include <gurobi_c++.h>

#include <cassert>
#include <filesystem>
#include <fstream>
#include <mapper/gurobi_routing_mapper.hpp>

mapper::GurobiILPRoutingMapper::GurobiILPRoutingMapper(
    const std::shared_ptr<entity::DFG> dfg_ptr,
    const std::shared_ptr<entity::MRRG> mrrg_ptr)
    : dfg_ptr_(dfg_ptr), mrrg_ptr_(mrrg_ptr) {}

mapper::GurobiILPRoutingMapper* mapper::GurobiILPRoutingMapper::CreateMapper(
    const std::shared_ptr<entity::DFG> dfg_ptr,
    const std::shared_ptr<entity::MRRG> mrrg_ptr) {
  mapper::GurobiILPRoutingMapper* result = new mapper::GurobiILPRoutingMapper;
  *result = mapper::GurobiILPRoutingMapper(dfg_ptr, mrrg_ptr);

  return result;
}

mapper::MappingResult mapper::GurobiILPRoutingMapper::Execution() {
  const auto start_time = std::chrono::system_clock::now();
  try {
    // create gurobi env
    GRBEnv env = GRBEnv(true);
    env.set(GRB_IntParam_Threads, 32);
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

    // create variable
    std::vector<std::vector<GRBVar>> map_op_to_PE(
        dfg_node_num, std::vector<GRBVar>(mrrg_node_num));
    std::vector<std::vector<GRBVar>> map_op_to_route(
        dfg_node_num, std::vector<GRBVar>(mrrg_edge_num));
    std::vector<GRBVar> map_route_op_to_PE(mrrg_node_num);

    for (int dfg_node_id = 0; dfg_node_id < dfg_node_num; dfg_node_id++) {
      for (int mrrg_node_id = 0; mrrg_node_id < mrrg_node_num; mrrg_node_id++) {
        std::string var_name;
        var_name = "f_" + std::to_string(dfg_node_id) + "_" +
                   std::to_string(mrrg_node_id);
        map_op_to_PE[dfg_node_id][mrrg_node_id] =
            model.addVar(0.0, 1.0, 0.0, GRB_BINARY, var_name);
      }
      for (int mrrg_edge_id = 0; mrrg_edge_id < mrrg_edge_num; mrrg_edge_id++) {
        std::string var_name;
        var_name = "r_" + std::to_string(dfg_node_id) + "_" +
                   std::to_string(mrrg_edge_id);
        map_op_to_route[dfg_node_id][mrrg_edge_id] =
            model.addVar(0.0, 1.0, 0.0, GRB_BINARY, var_name);
      }
    }

    for (int mrrg_node_id = 0; mrrg_node_id < mrrg_node_num; mrrg_node_id++) {
      std::string var_name;
      var_name = "rp_" + std::to_string(mrrg_node_id);
      map_route_op_to_PE[mrrg_node_id] =
          model.addVar(0.0, 1.0, 0.0, GRB_BINARY, var_name);
    }

    // set objective
    GRBLinExpr objective_lin_expr;
    const double object_coefficient = 1.0;
    for (int dfg_node_id = 0; dfg_node_id < dfg_node_num; dfg_node_id++) {
      for (int mrrg_edge_id = 0; mrrg_edge_id < mrrg_edge_num; mrrg_edge_id++) {
        objective_lin_expr.addTerms(
            &object_coefficient, &(map_op_to_route[dfg_node_id][mrrg_edge_id]),
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

    // add constraint: route operation placement
    const double route_op_placement_coefficient = 1;
    for (int mrrg_node_id = 0; mrrg_node_id < mrrg_node_num; mrrg_node_id++) {
      std::vector<int> mrrg_in_edge_ids =
          mrrg_ptr_->GetInEdgeIdVec(mrrg_node_id);
      GRBLinExpr tmp_lin_expr;
      for (int dfg_node_id = 0; dfg_node_id < dfg_node_num; dfg_node_id++) {
        for (int mrrg_in_edge_id : mrrg_in_edge_ids) {
          tmp_lin_expr.addTerms(
              &route_op_placement_coefficient,
              &(map_op_to_route[dfg_node_id][mrrg_in_edge_id]), 1);
        }
      }
      std::string constr_name =
          "c_route_op_placement_" + std::to_string(mrrg_node_id);
      model.addConstr(map_route_op_to_PE[mrrg_node_id], GRB_LESS_EQUAL,
                      tmp_lin_expr, constr_name);
    }

    // add constraint: functional unit exclusivity
    const double exclusivity_coefficient = 1;
    for (int mrrg_node_id = 0; mrrg_node_id < mrrg_node_num; mrrg_node_id++) {
      GRBLinExpr tmp_lin_expr;
      for (int dfg_node_id = 0; dfg_node_id < dfg_node_num; dfg_node_id++) {
        tmp_lin_expr.addTerms(&exclusivity_coefficient,
                              &(map_op_to_PE[dfg_node_id][mrrg_node_id]), 1);
      }
      tmp_lin_expr.addTerms(&exclusivity_coefficient,
                            &(map_route_op_to_PE[mrrg_node_id]), 1);
      std::string constr_name = "c_exclusivity_" + std::to_string(mrrg_node_id);
      model.addConstr(tmp_lin_expr, GRB_LESS_EQUAL, 1, constr_name);
    }

    // add constraint: route operation exclusivity
    const double route_exclusivity_coefficient = 1;
    for (int mrrg_edge_id = 0; mrrg_edge_id < mrrg_edge_num; mrrg_edge_id++) {
      GRBLinExpr tmp_lin_expr;
      for (int dfg_node_id = 0; dfg_node_id < dfg_node_num; dfg_node_id++) {
        tmp_lin_expr.addTerms(&route_exclusivity_coefficient,
                              &(map_op_to_route[dfg_node_id][mrrg_edge_id]), 1);
      }
      std::string constr_name =
          "c_route_exclusivity_" + std::to_string(mrrg_edge_id);
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

    // add constraint: fanout
    const double fanout_coefficient = 1;
    for (int dfg_node_id = 0; dfg_node_id < dfg_node_num; dfg_node_id++) {
      int dfg_output_num =
          static_cast<int>(dfg_ptr_->GetAdjacentNodeIdVec(dfg_node_id).size());
      for (int mrrg_node_id = 0; mrrg_node_id < mrrg_node_num; mrrg_node_id++) {
        std::vector<int> mrrg_adj_edge_ids =
            mrrg_ptr_->GetOutEdgeIdVec(mrrg_node_id);
        GRBLinExpr tmp_lin_expr;
        for (int mrrg_adj_edge_id : mrrg_adj_edge_ids) {
          tmp_lin_expr.addTerms(
              &fanout_coefficient,
              &(map_op_to_route[dfg_node_id][mrrg_adj_edge_id]), 1);
        }
        std::string constr_name = "c_fanout_" + std::to_string(dfg_node_id) +
                                  "_" + std::to_string(mrrg_node_id);
        model.addConstr(map_op_to_PE[dfg_node_id][mrrg_node_id], GRB_LESS_EQUAL,
                        tmp_lin_expr, constr_name);
      }
    }

    // add constraint: fanin
    const double fanin_coefficient = 1;
    for (int dfg_node_id = 0; dfg_node_id < dfg_node_num; dfg_node_id++) {
      int dfg_input_num =
          static_cast<int>(dfg_ptr_->GetParentNodeIdVec(dfg_node_id).size());
      for (int mrrg_node_id = 0; mrrg_node_id < mrrg_node_num; mrrg_node_id++) {
        std::vector<int> mrrg_parent_edge_ids =
            mrrg_ptr_->GetInEdgeIdVec(mrrg_node_id);
        GRBLinExpr tmp_lin_expr;
        for (int mrrg_parent_edge_id : mrrg_parent_edge_ids) {
          tmp_lin_expr.addTerms(
              &fanin_coefficient,
              &(map_op_to_route[dfg_node_id][mrrg_parent_edge_id]), 1);
        }
        std::string constr_name = "c_fanin_" + std::to_string(dfg_node_id) +
                                  "_" + std::to_string(mrrg_node_id);
        model.addConstr(dfg_input_num * map_op_to_PE[dfg_node_id][mrrg_node_id],
                        GRB_EQUAL, tmp_lin_expr, constr_name);
      }
    }

    // add constraint: map_op_to_route consistency
    const double map_op_to_route_coefficient = 1;
    for (int mrrg_node_id = 0; mrrg_node_id < mrrg_node_num; mrrg_node_id++) {
      std::vector<int> mrrg_in_edge_ids =
          mrrg_ptr_->GetInEdgeIdVec(mrrg_node_id);
      std::vector<int> mrrg_out_edge_ids =
          mrrg_ptr_->GetOutEdgeIdVec(mrrg_node_id);
      for (int dfg_node_id = 0; dfg_node_id < dfg_node_num; dfg_node_id++) {
        GRBLinExpr in_edge_num;
        GRBLinExpr out_edge_num;
        for (int mrrg_in_edge_id : mrrg_in_edge_ids) {
          in_edge_num.addTerms(&map_op_to_route_coefficient,
                               &(map_op_to_route[dfg_node_id][mrrg_in_edge_id]),
                               1);
        }
        for (int mrrg_out_edge_id : mrrg_out_edge_ids) {
          out_edge_num.addTerms(
              &map_op_to_route_coefficient,
              &(map_op_to_route[dfg_node_id][mrrg_out_edge_id]), 1);
        }
        std::string constr_name = "c_map_op_to_route_" +
                                  std::to_string(dfg_node_id) + "_" +
                                  std::to_string(mrrg_node_id);
        model.addConstr(map_route_op_to_PE[mrrg_node_id], GRB_LESS_EQUAL,
                        out_edge_num, constr_name);
        model.addConstr(in_edge_num, GRB_LESS_EQUAL, out_edge_num, constr_name);
      }
    }

    // add constraint: aporopriate input type
    const double appropriate_input_type_coefficient = 1;
    for (int dfg_node_id = 0; dfg_node_id < dfg_node_num; dfg_node_id++) {
      std::vector<int> dfg_parent_node_id_vec =
          dfg_ptr_->GetParentNodeIdVec(dfg_node_id);
      int dfg_parent_node_num = static_cast<int>(dfg_parent_node_id_vec.size());
      for (int mrrg_node_id = 0; mrrg_node_id < mrrg_node_num; mrrg_node_id++) {
        std::vector<int> mrrg_parent_edge_ids =
            mrrg_ptr_->GetInEdgeIdVec(mrrg_node_id);
        for (int dfg_parent_node_id : dfg_parent_node_id_vec) {
          GRBLinExpr tmp_lin_expr;
          for (int mrrg_parent_edge_id : mrrg_parent_edge_ids) {
            tmp_lin_expr.addTerms(
                &appropriate_input_type_coefficient,
                &(map_op_to_route[dfg_parent_node_id][mrrg_parent_edge_id]),
                1);
          }

          std::string constr_name = "c_appropriate_input_type_" +
                                    std::to_string(dfg_node_id) + "_" +
                                    std::to_string(mrrg_node_id) + "_" +
                                    std::to_string(dfg_parent_node_id);
          model.addConstr(map_op_to_PE[dfg_node_id][mrrg_node_id], GRB_EQUAL,
                          tmp_lin_expr, constr_name);
        }
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

    if (status != GRB_OPTIMAL && status != GRB_SUBOPTIMAL) {
      if (status == GRB_INFEASIBLE) {
        model.computeIIS();
        model.write("debug.iis");
      }
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
      for (int j = 0; j < mrrg_node_num; j++) {
        if (map_op_to_PE[i][j].get(GRB_DoubleAttr_X) == 1) {
          dfg_node_to_mrrg_node[i] = j;
        }
      }
      for (int j = 0; j < mrrg_edge_num; j++) {
        if (map_op_to_route[i][j].get(GRB_DoubleAttr_X) == 1) {
          dfg_output_to_mrrg_edge[i].push_back(j);
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

void mapper::GurobiILPRoutingMapper::SetLogFilePath(
    const std::string& log_file_path) {
  log_file_path_ = log_file_path;
  return;
}

void mapper::GurobiILPRoutingMapper::SetTimeOut(double timeout_s) {
  timeout_s_ = timeout_s;
  return;
}