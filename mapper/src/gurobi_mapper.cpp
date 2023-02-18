#include <gurobi_c++.h>

#include <mapper/gurobi_mapper.hpp>
#include <string>

mapper::GurobiILPMapper::GurobiILPMapper(
    const std::shared_ptr<entity::DFG> dfg_ptr,
    const std::shared_ptr<entity::MRRG> mrrg_ptr)
    : dfg_ptr_(dfg_ptr), mrrg_ptr_(mrrg_ptr) {}

mapper::IILPMapper* mapper::GurobiILPMapper::CreateMapper(
    const std::shared_ptr<entity::DFG> dfg_ptr,
    const std::shared_ptr<entity::MRRG> mrrg_ptr) {
  mapper::GurobiILPMapper* result_mapper;
  *result_mapper = mapper::GurobiILPMapper(dfg_ptr, mrrg_ptr);

  return result_mapper;
}

entity::Mapping mapper::GurobiILPMapper::Execution() {
  try {
    // create gurobi env
    GRBEnv env = GRBEnv(true);
    env.start();

    // create an empty model
    GRBModel model = GRBModel(env);

    int dfg_node_num = dfg_ptr_->GetNodeNum();
    int mrrg_node_num = mrrg_ptr_->GetNodeNum();

    // create variable
    std::vector<std::vector<GRBVar>> map_op_to_PE(
        dfg_node_num, std::vector<GRBVar>(mrrg_node_num));
    std::vector<std::vector<GRBVar>> map_output_to_PE_reg(
        dfg_node_num, std::vector<GRBVar>(mrrg_node_num));

    for (int i = 0; i < dfg_node_num; i++) {
      for (int j = 0; j < mrrg_node_num; j++) {
        std::string var_name;
        var_name = "f_" + std::to_string(i) + "_" + std::to_string(j);
        map_op_to_PE[i][j] = model.addVar(0.0, 1.0, 0.0, GRB_BINARY, var_name);

        var_name = "r_" + std::to_string(i) + "_" + std::to_string(j);
        map_output_to_PE_reg[i][j] =
            model.addVar(0.0, 1.0, 0.0, GRB_BINARY, var_name);
      }
    }

    // set objective
    GRBLinExpr objective_lin_expr;
    const double object_coefficient = 1.0;
    for (int i = 0; i < dfg_node_num; i++) {
      for (int j = 0; j < mrrg_node_num; j++) {
        objective_lin_expr.addTerms(&object_coefficient,
                                    &(map_output_to_PE_reg[i][j]), 1);
      }
    }
    model.setObjective(objective_lin_expr, GRB_MAXIMIZE);

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
                &(map_output_to_PE_reg[dfg_node_id][mrrg_adj_node_id]), 1);
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
          model.addConstr(map_output_to_PE_reg[dfg_node_id][mrrg_node_id],
                          GRB_LESS_EQUAL, tmp_lin_expr, constr_name);
        }
      }
    }

    // add constraint: reg size
    const double reg_size_coefficient = 1;
    for (int i = 0; i < mrrg_node_num; i++) {
      GRBLinExpr tmp_lin_expr;
      for (int j = 0; j < dfg_node_num; j++) {
        tmp_lin_expr.addTerms(&reg_size_coefficient,
                              &(map_output_to_PE_reg[j][i]), 1);
      }
      std::string constr_name = "c_reg_size_" + std::to_string(i);
      int local_reg_size = (mrrg_ptr_->GetNodeProperty(i)).local_reg_size;
      model.addConstr(tmp_lin_expr, GRB_LESS_EQUAL, local_reg_size,
                      constr_name);
    }

    // add constraint: elastic order (TODO)

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
        if (map_output_to_PE_reg[i][j].get(GRB_DoubleAttr_X) == 1) {
          dfg_output_to_mrrg_reg[i].push_back(j);
        }
      }
    }

    return entity::Mapping(*mrrg_ptr_, *dfg_ptr_, dfg_node_to_mrrg_node,
                           dfg_output_to_mrrg_reg);
  } catch (GRBException e) {
    std::cout << "Error code = " << e.getErrorCode() << std::endl;
    std::cout << e.getMessage() << std::endl;

    return entity::Mapping(false);
  }
}
