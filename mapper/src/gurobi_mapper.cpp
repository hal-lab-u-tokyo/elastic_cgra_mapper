#include <gurobi_c++.h>

#include <cassert>
#include <filesystem>
#include <fstream>
#include <mapper/gurobi_mapper.hpp>

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

std::pair<bool, entity::Mapping> mapper::GurobiILPMapper::Execution() {
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

    int dfg_node_num = dfg_ptr_->GetNodeNum();
    int mrrg_node_num = mrrg_ptr_->GetNodeNum();

    // create variable
    std::vector<std::vector<GRBVar>> map_op_to_PE(
        dfg_node_num, std::vector<GRBVar>(mrrg_node_num));
    std::vector<std::vector<GRBVar>> map_output_to_route(
        dfg_node_num, std::vector<GRBVar>(mrrg_node_num));

    for (int i = 0; i < dfg_node_num; i++) {
      for (int j = 0; j < mrrg_node_num; j++) {
        std::string var_name;
        var_name = "f_" + std::to_string(i) + "_" + std::to_string(j);
        map_op_to_PE[i][j] = model.addVar(0.0, 1.0, 0.0, GRB_BINARY, var_name);

        var_name = "r_" + std::to_string(i) + "_" + std::to_string(j);
        map_output_to_route[i][j] =
            model.addVar(0.0, 1.0, 0.0, GRB_BINARY, var_name);
      }
    }
    // set objective
    GRBLinExpr objective_lin_expr;
    const double object_coefficient = 1.0;
    for (int i = 0; i < dfg_node_num; i++) {
      for (int j = 0; j < mrrg_node_num; j++) {
        objective_lin_expr.addTerms(&object_coefficient,
                                    &(map_output_to_route[i][j]), 1);
      }
    }
    model.setObjective(objective_lin_expr, GRB_MINIMIZE);

    // add constraint
    // add constraint: operation placement
    const double placement_coefficient = 1;
    for (int i = 0; i < dfg_node_num; i++) {
      GRBLinExpr tmp_lin_expr;
      for (int j = 0; j < mrrg_node_num; j++) {
        tmp_lin_expr.addTerms(&placement_coefficient, &(map_op_to_PE[i][j]), 1);
      }
      std::string constr_name = "c_placement_" + std::to_string(i);
      model.addConstr(tmp_lin_expr, GRB_EQUAL, 1, constr_name);
    }

    // add constraint: functional unit exclusivity
    const double exclusivity_coefficient = 1;
    for (int i = 0; i < mrrg_node_num; i++) {
      GRBLinExpr tmp_lin_expr;
      for (int j = 0; j < dfg_node_num; j++) {
        tmp_lin_expr.addTerms(&exclusivity_coefficient, &(map_op_to_PE[j][i]),
                              1);
        tmp_lin_expr.addTerms(&exclusivity_coefficient,
                              &(map_output_to_route[j][i]), 1);
      }
      std::string constr_name = "c_exclusivity_" + std::to_string(i);
      model.addConstr(tmp_lin_expr, GRB_LESS_EQUAL, 1, constr_name);
    }

    // add constraint: functional unit legality
    for (int i = 0; i < dfg_node_num; i++) {
      for (int j = 0; j < mrrg_node_num; j++) {
        entity::OpType dfg_op = (dfg_ptr_->GetNodeProperty(i)).op;
        std::vector<entity::OpType> mrrg_supported_op_vec =
            (mrrg_ptr_->GetNodeProperty(j)).supported_operations;

        bool support_op = false;
        for (entity::OpType supported_op : mrrg_supported_op_vec) {
          if (dfg_op == supported_op) support_op = true;
        }

        if (!support_op) {
          std::string constr_name =
              "c_legality_" + std::to_string(i) + "_" + std::to_string(j);
          model.addConstr(map_op_to_PE[i][j], GRB_EQUAL, 0, constr_name);
        }
      }
    }

    // add constraint: data flow
    const double data_flow_coefficient = 1;
    for (int dfg_node_id = 0; dfg_node_id < dfg_node_num; dfg_node_id++) {
      std::vector<int> dfg_adjacent_node_ids =
          dfg_ptr_->GetAdjacentNodeIdVec(dfg_node_id);
      for (int mrrg_node_id = 0; mrrg_node_id < mrrg_node_num; mrrg_node_id++) {
        std::vector<int> mrrg_adjacent_node_ids =
            mrrg_ptr_->GetAdjacentNodeIdVec(mrrg_node_id);
        for (int dfg_adj_node_id : dfg_adjacent_node_ids) {
          GRBLinExpr tmp_lin_expr;
          for (int mrrg_adj_node_id : mrrg_adjacent_node_ids) {
            tmp_lin_expr.addTerms(
                &data_flow_coefficient,
                &(map_op_to_PE[dfg_adj_node_id][mrrg_adj_node_id]), 1);
            tmp_lin_expr.addTerms(
                &data_flow_coefficient,
                &(map_output_to_route[dfg_node_id][mrrg_adj_node_id]), 1);
          }
          std::string constr_name;
          constr_name = "c_data_flow_" + std::to_string(dfg_node_id) + "_" +
                        std::to_string(mrrg_node_id) + "_" +
                        std::to_string(dfg_adj_node_id);
          model.addConstr(map_op_to_PE[dfg_node_id][mrrg_node_id],
                          GRB_LESS_EQUAL, tmp_lin_expr, constr_name);

          constr_name = "c_data_flow_reg_" + std::to_string(dfg_node_id) + "_" +
                        std::to_string(mrrg_node_id) + "_" +
                        std::to_string(dfg_adj_node_id);
          model.addConstr(map_output_to_route[dfg_node_id][mrrg_node_id],
                          GRB_LESS_EQUAL, tmp_lin_expr, constr_name);
        }
      }
    }

    // add constraint: adequate number of input
    const double adequate_number_coefficient = 1;
    for (int dfg_node_id = 0; dfg_node_id < dfg_node_num; dfg_node_id++) {
      for (int mrrg_node_id = 0; mrrg_node_id < mrrg_node_num; mrrg_node_id++) {
        std::vector<int> dfg_parent_node_id_vec =
            dfg_ptr_->GetParentNodeIdVec(dfg_node_id);
        std::vector<int> mrrg_parent_node_id_vec =
            mrrg_ptr_->GetParentNodeIdVec(mrrg_node_id);

        GRBLinExpr parent_node_num_expr;
        double dfg_parent_node_num =
            static_cast<double>(dfg_parent_node_id_vec.size());
        parent_node_num_expr.addTerms(
            &dfg_parent_node_num, &(map_op_to_PE[dfg_node_id][mrrg_node_id]),
            1);
        GRBLinExpr neighborhood_parent_node_num_lin_expr;
        for (int dfg_parent_node_id : dfg_parent_node_id_vec) {
          for (int mrrg_parent_node_id : mrrg_parent_node_id_vec) {
            neighborhood_parent_node_num_lin_expr.addTerms(
                &adequate_number_coefficient,
                &(map_op_to_PE[dfg_parent_node_id][mrrg_parent_node_id]), 1);
            neighborhood_parent_node_num_lin_expr.addTerms(
                &adequate_number_coefficient,
                &(map_output_to_route[dfg_parent_node_id][mrrg_parent_node_id]),
                1);
          }
        }

        model.addConstr(parent_node_num_expr, GRB_LESS_EQUAL,
                        neighborhood_parent_node_num_lin_expr);
      }
    }

    // add constraint for elastic CGRA
    if (mrrg_ptr_->GetMRRGConfig().cgra_type ==
        entity::MRRGCGRAType::kElastic) {
      // add constraint: Race Condition Avoidance
      entity::MRRGConfig mrrg_config = mrrg_ptr_->GetMRRGConfig();
      for (int src_node_id = 0; src_node_id < dfg_node_num; src_node_id++) {
        for (int to_node_id = 0; to_node_id < dfg_node_num; to_node_id++) {
          if (src_node_id == to_node_id) continue;

          bool is_reachable = dfg_ptr_->IsReachable(src_node_id, to_node_id);
          if (!is_reachable) continue;

          for (int row_id = 0; row_id < mrrg_config.row; row_id++) {
            for (int column_id = 0; column_id < mrrg_config.column;
                 column_id++) {
              for (int earlier_context_id = 0;
                   earlier_context_id < mrrg_config.context_size - 1;
                   earlier_context_id++) {
                int earlier_mrrg_node_id = mrrg_ptr_->GetMRRGNodeId(
                    row_id, column_id, earlier_context_id);
                for (int later_context_id = earlier_context_id + 1;
                     later_context_id < mrrg_config.context_size;
                     later_context_id++) {
                  int later_mrrg_node_id = mrrg_ptr_->GetMRRGNodeId(
                      row_id, column_id, later_context_id);

                  std::string constr_name =
                      "c_elastic_race_condition_" + std::to_string(row_id) +
                      "_" + std::to_string(column_id) + "_" +
                      std::to_string(later_context_id) + "_" +
                      std::to_string(earlier_context_id);
                  model.addConstr(
                      map_op_to_PE[src_node_id][later_mrrg_node_id],
                      GRB_LESS_EQUAL,
                      1 - map_op_to_PE[to_node_id][earlier_mrrg_node_id],
                      constr_name + "_op_to_op");
                  model.addConstr(
                      map_op_to_PE[src_node_id][later_mrrg_node_id],
                      GRB_LESS_EQUAL,
                      1 - map_output_to_route[to_node_id][earlier_mrrg_node_id],
                      constr_name + "_op_to_route");
                  model.addConstr(
                      map_output_to_route[src_node_id][later_mrrg_node_id],
                      GRB_LESS_EQUAL,
                      1 - map_op_to_PE[to_node_id][earlier_mrrg_node_id],
                      constr_name + "_route_to_op");
                  model.addConstr(
                      map_output_to_route[src_node_id][later_mrrg_node_id],
                      GRB_LESS_EQUAL,
                      1 - map_output_to_route[to_node_id][earlier_mrrg_node_id],
                      constr_name + "_route_to_route");
                }
              }
            }
          }
        }
      }

      // add constraint: context collapsing
      const double context_collapsing_coefficient = 1;
      for (int row_id = 0; row_id < mrrg_config.row; row_id++) {
        for (int column_id = 0; column_id < mrrg_config.column; column_id++) {
          for (int no_op_context_id = 0;
               no_op_context_id < mrrg_config.context_size - 1;
               no_op_context_id++) {
            GRBLinExpr num_op_in_config;
            int no_op_mrrg_node_id =
                mrrg_ptr_->GetMRRGNodeId(row_id, column_id, no_op_context_id);
            for (int dfg_node_id = 0; dfg_node_id < dfg_node_num;
                 dfg_node_id++) {
              num_op_in_config.addTerms(
                  &context_collapsing_coefficient,
                  &(map_op_to_PE[dfg_node_id][no_op_mrrg_node_id]), 1);
              num_op_in_config.addTerms(
                  &context_collapsing_coefficient,
                  &(map_output_to_route[dfg_node_id][no_op_mrrg_node_id]), 1);
            }

            GRBLinExpr later_num_op_in_config;
            for (int dfg_node_id = 0; dfg_node_id < dfg_node_num;
                 dfg_node_id++) {
              int later_mrrg_node_id = mrrg_ptr_->GetMRRGNodeId(
                  row_id, column_id, no_op_context_id + 1);
              later_num_op_in_config.addTerms(
                  &context_collapsing_coefficient,
                  &(map_op_to_PE[dfg_node_id][later_mrrg_node_id]), 1);
              later_num_op_in_config.addTerms(
                  &context_collapsing_coefficient,
                  &(map_output_to_route[dfg_node_id][later_mrrg_node_id]), 1);
            }

            std::string constr_name = "c_context_collapsing_" +
                                      std::to_string(row_id) + "_" +
                                      std::to_string(column_id) + "_" +
                                      std::to_string(no_op_context_id);
            model.addConstr(num_op_in_config, GRB_GREATER_EQUAL,
                            later_num_op_in_config, constr_name);
          }
        }
      }
    }

    // optimize model
    model.optimize();

    // get result
    std::vector<int> dfg_node_to_mrrg_node(dfg_node_num);
    std::vector<std::vector<int>> dfg_output_to_mrrg_reg(dfg_node_num);

    for (int i = 0; i < dfg_node_num; i++) {
      for (int j = 0; j < mrrg_node_num; j++) {
        if (map_op_to_PE[i][j].get(GRB_DoubleAttr_X) == 1) {
          dfg_node_to_mrrg_node[i] = j;
        }
        if (map_output_to_route[i][j].get(GRB_DoubleAttr_X) == 1) {
          dfg_output_to_mrrg_reg[i].push_back(j);
        }
      }
    }

    return std::make_pair(
        true, entity::Mapping(*mrrg_ptr_, *dfg_ptr_, dfg_node_to_mrrg_node,
                              dfg_output_to_mrrg_reg));
  } catch (GRBException e) {
    std::cout << "Error code = " << e.getErrorCode() << std::endl;
    std::cout << e.getMessage() << std::endl;

    return std::make_pair(false, entity::Mapping(mrrg_ptr_->GetMRRGConfig()));
  }
}

void mapper::GurobiILPMapper::SetLogFilePath(const std::string& log_file_path) {
  log_file_path_ = log_file_path;

  std::ofstream log_file;
  const auto mrrg_config = mrrg_ptr_->GetMRRGConfig();
  log_file.open(log_file_path_.value());

  log_file << "-- CGRA setting --" << std::endl;
  log_file << "row: " << mrrg_config.row << std::endl;
  log_file << "column: " << mrrg_config.column << std::endl;
  log_file << "context_size: " << mrrg_config.context_size << std::endl;
  log_file << "memory_io: "
           << entity::MRRGMemoryIoTypeToString(mrrg_config.memory_io)
           << std::endl;
  log_file << "cgra_type: "
           << entity::MRRGCGRATypeToString(mrrg_config.cgra_type) << std::endl;
  log_file << "network_type: "
           << entity::MRRGNetworkTypeToString(mrrg_config.network_type)
           << std::endl;

  log_file.close();
  return;
}
}