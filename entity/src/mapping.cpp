#include <entity/mapping.hpp>
#include <queue>

entity::Mapping::Mapping(
    entity::MRRG mrrg, entity::DFG dfg,
    const std::vector<int>& dfg_node_to_mrrg_node,
    const std::vector<std::vector<int>>& dfg_output_to_mrrg_reg)
    : config_map_({}) {
  mrrg_config_ = mrrg.GetMRRGConfig();

  // create config_map_
  for (int from_op_id = 0; from_op_id < dfg.GetNodeNum(); from_op_id++) {
    int from_op_PE_id = dfg_node_to_mrrg_node[from_op_id];

    std::vector<int> PE_id_vec;
    std::unordered_map<int, int> PE_id_to_op_id_map = {
        {from_op_PE_id, from_op_id}};

    // create PE_id_vec
    std::vector<int> to_op_id_vec = dfg.GetAdjacentNodeIdVec(from_op_id);
    for (int to_op_id : to_op_id_vec) {
      int to_PE_id = dfg_node_to_mrrg_node[to_op_id];
      PE_id_to_op_id_map.emplace(to_PE_id, to_op_id);
      PE_id_vec.emplace_back(to_PE_id);
    }

    const int kRouteOpId = -1;
    std::vector<int> op_result_mrrg_reg_vec = dfg_output_to_mrrg_reg[from_op_id];
    for (int mrrg_reg_id : op_result_mrrg_reg_vec) {
      PE_id_to_op_id_map.emplace(mrrg_reg_id, kRouteOpId);
      PE_id_vec.emplace_back(mrrg_reg_id);
    }

    auto GetOpTypeFromPEId = [&](int PE_id) {
      int op_id = PE_id_to_op_id_map.at(PE_id);
      if (op_id == kRouteOpId) {
        return entity::OpType::ROUTE;
      }
      return dfg.GetNodeProperty(op_id).op;
    };

    auto GetOpNameFromPEId = [&](int PE_id) {
      int op_id = PE_id_to_op_id_map.at(PE_id);
      if (op_id == kRouteOpId) {
        return std::string("route");
      }
      return dfg.GetNodeProperty(op_id).op_name;
    };

    // BFS
    std::queue<int> from_PE_id_queue;
    from_PE_id_queue.push(from_op_PE_id);
    std::set<int> searched_PE_id;
    while (from_PE_id_queue.size() > 0) {
      int from_PE_id = from_PE_id_queue.front();
      from_PE_id_queue.pop();

      std::vector<int> adj_PE_id_vec = mrrg.GetAdjacentNodeIdVec(from_PE_id);
      entity::ConfigId from_config_id(mrrg.GetNodeProperty(from_PE_id));
      entity::OpType from_op_type = GetOpTypeFromPEId(from_PE_id);
      std::string from_op_name = GetOpNameFromPEId(from_PE_id);

      if (config_map_.count(from_config_id) == 0) {
        config_map_.emplace(from_config_id, entity::CGRAConfig(from_op_type, from_op_name));
      }
      if (from_op_type == entity::OpType::CONST) {
        int op_id = PE_id_to_op_id_map.at(from_PE_id);
        if (dfg.GetNodeProperty(op_id).const_value.has_value()) {
          config_map_[from_config_id].SetConstValue(
              dfg.GetNodeProperty(op_id).const_value.value());
        }
      }

      for (int adj_PE_id : adj_PE_id_vec) {
        if (searched_PE_id.count(adj_PE_id) > 0) {
          continue;
        }
        for (int to_PE_id : PE_id_vec) {
          if (adj_PE_id != to_PE_id) continue;

          entity::ConfigId to_config_id(mrrg.GetNodeProperty(to_PE_id));
          entity::OpType to_op_type = GetOpTypeFromPEId(to_PE_id);
          std::string to_op_name = GetOpNameFromPEId(to_PE_id);

          if (config_map_.count(to_config_id) == 0) {
            config_map_.emplace(
                to_config_id, entity::CGRAConfig::GenerateInitialCGRAConfig());
          }
          config_map_[from_config_id].AddToConfig(to_config_id, from_op_type,
                                                  from_op_name);
          config_map_[to_config_id].AddFromConfig(from_config_id, to_op_type,
                                                  to_op_name);
          if (searched_PE_id.count(adj_PE_id) == 0) {
            from_PE_id_queue.push(to_PE_id);
            searched_PE_id.emplace(to_PE_id);
          }
        }
      }
    }
  }
};

size_t entity::Mapping::GetOpNum() const {
  size_t result = 0;
  for (const auto& id_and_config : config_map_) {
    if (id_and_config.second.operation_type != entity::OpType::NOP) {
      result++;
    }
  }

  return result;
}
