#include <entity/mapping.hpp>

entity::Mapping::Mapping(
    entity::MRRG mrrg, entity::DFG dfg,
    const std::vector<int>& dfg_node_to_mrrg_node,
    const std::vector<std::vector<int>>& dfg_output_to_mrrg_reg)
    : config_id_to_dfg_node_id_({}), config_map_({}) {
  mrrg_ptr_ = std::make_shared<entity::MRRG>();
  dfg_ptr_ = std::make_shared<entity::DFG>();
  is_succeed_ = true;

  *mrrg_ptr_ = mrrg;
  *dfg_ptr_ = dfg;

  // create config_id_to_dfg_node_id_
  for (int i = 0; i < dfg.GetNodeNum(); i++) {
    int mrrg_id = dfg_node_to_mrrg_node[i];
    entity::MRRGNodeProperty mrrg_node_property =
        mrrg_ptr_->GetNodeProperty(mrrg_id);
    entity::ConfigId config_id(mrrg_node_property);
    config_id_to_dfg_node_id_.emplace(config_id, i);
  }

  // create config_map_
  for (int from_op_id = 0; from_op_id < dfg.GetNodeNum(); from_op_id++) {
    int from_PE_id = dfg_node_to_mrrg_node[from_op_id];

    std::vector<int> PE_id_vec{from_PE_id};
    std::unordered_map<int, int> PE_id_to_op_id_map = {
        {from_PE_id, from_op_id}};

    // create PE_id_vec
    std::vector<int> to_op_id_vec = dfg_ptr_->GetAdjacentNodeIdVec(from_op_id);
    for (int to_op_id : to_op_id_vec) {
      int to_PE_id = dfg_node_to_mrrg_node[to_op_id];
      PE_id_to_op_id_map.emplace(to_PE_id, to_op_id);
      PE_id_vec.emplace_back(to_PE_id);
    }

    const int kNopId = -1;
    std::vector<int> op_result_mrrg_reg_vec =
        dfg_output_to_mrrg_reg[from_op_id];
    for (int mrrg_reg_id : op_result_mrrg_reg_vec) {
      PE_id_to_op_id_map.emplace(mrrg_reg_id, kNopId);
      PE_id_vec.emplace_back(mrrg_reg_id);
    }

    auto GetOpTypeFromPEId = [&](int PE_id) {
      int op_id = PE_id_to_op_id_map.at(PE_id);
      if (op_id == kNopId) {
        return entity::OpType::NOP;
      }
      return dfg_ptr_->GetNodeProperty(op_id).op;
    };

    for (int from_PE_id : PE_id_vec) {
      std::vector<int> adj_PE_id_vec =
          mrrg_ptr_->GetAdjacentNodeIdVec(from_PE_id);
      entity::ConfigId from_config_id(mrrg_ptr_->GetNodeProperty(from_PE_id));
      entity::OpType from_op_type = GetOpTypeFromPEId(from_PE_id);
      for (int adj_PE_id : adj_PE_id_vec) {
        for (int to_PE_id : PE_id_vec) {
          if (adj_PE_id == to_PE_id) {
            entity::ConfigId to_config_id(mrrg_ptr_->GetNodeProperty(to_PE_id));
            entity::OpType to_op_type = GetOpTypeFromPEId(to_PE_id);

            if (config_map_.count(from_config_id) == 0) {
              config_map_.emplace(
                  from_config_id,
                  entity::CGRAConfig::GenerateInitialCGRAConfig());
            }
            if (config_map_.count(to_config_id) == 0) {
              config_map_.emplace(
                  to_config_id,
                  entity::CGRAConfig::GenerateInitialCGRAConfig());
            }

            config_map_[from_config_id].AddToConfig(to_config_id, to_op_type);
            config_map_[to_config_id].AddFromConfig(from_config_id, to_op_type);
          }
        }
      }
    }
  }
};

entity::Mapping::Mapping(bool is_succeed)
    : config_id_to_dfg_node_id_({}), config_map_({}) {
  mrrg_ptr_ = std::make_shared<entity::MRRG>();
  dfg_ptr_ = std::make_shared<entity::DFG>();

  is_succeed_ = is_succeed;
};