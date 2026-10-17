#include <entity/dfg.hpp>
#include <queue>

entity::DFG::DFG(entity::DFGGraph dfg_graph)
    : entity::BaseGraphClass<entity::DFGNodeProperty, entity::DFGEdgeProperty,
                             entity::DFGGraphProperty>(dfg_graph){};

std::vector<int> entity::DFG::Execute(std::vector<int>& memory, int iterations,
                                      int output_node_id) {
  std::vector<int> output_value_vec;
  std::vector<int> dfg_node_id_vec;
  std::vector<std::vector<int>> dfg_id_to_output_vec_map(GetNodeNum(),
                                                         std::vector<int>());
  for (int i = 0; i < GetNodeNum(); i++) {
    dfg_node_id_vec.push_back(i);
  }

  for (int i = 0; i < GetNodeNum(); i++) {
    entity::DFGNodeProperty node_property = GetNodeProperty(i);
    std::vector<int> child_node_id_vec = GetAdjacentNodeIdVec(i);
    std::unordered_set<int> child_node_ids = std::unordered_set<int>(
        child_node_id_vec.begin(), child_node_id_vec.end());

    if (node_property.op == entity::OpType::kRoute) {
      int tmp_node_id = i;
      while (GetNodeProperty(tmp_node_id).op == entity::OpType::kRoute) {
        std::vector<int> parent_node_id_vec = GetParentNodeIdVec(tmp_node_id);
        tmp_node_id = parent_node_id_vec[0];
      }

      if (child_node_ids.count(tmp_node_id) > 0) {
        dfg_id_to_output_vec_map[i].push_back(0);
      }
      break;
    } else {
      if (child_node_ids.count(i) > 0) {
        dfg_id_to_output_vec_map[i].push_back(0);
      }
    }
  }

  for (int i = 0; i < iterations; i++) {
    std::queue<int> dfg_id_queue(
        std::deque<int>(dfg_node_id_vec.begin(), dfg_node_id_vec.end()));

    std::unordered_set<int> searched_dfg_id_set;

    while (dfg_id_queue.size() > 0) {
      int current_dfg_id = dfg_id_queue.front();
      dfg_id_queue.pop();

      if (searched_dfg_id_set.count(current_dfg_id) > 0) {
        continue;
      }

      bool all_parents_searched = true;
      std::vector<int> parent_node_id_vec = GetParentNodeIdVec(current_dfg_id);
      for (const auto& from_config_id : parent_node_id_vec) {
        if (dfg_id_to_output_vec_map[from_config_id].size() <= i) {
          all_parents_searched = false;
          break;
        }
      }

      if (!all_parents_searched) {
        dfg_id_queue.push(current_dfg_id);
        continue;
      }

      DFGNodeProperty current_node_property = GetNodeProperty(current_dfg_id);
      int input1 = parent_node_id_vec.size() > 0
                       ? dfg_id_to_output_vec_map[parent_node_id_vec[0]][i]
                       : 0;
      int input2 = parent_node_id_vec.size() > 1
                       ? dfg_id_to_output_vec_map[parent_node_id_vec[1]][i]
                       : 0;
      int output_value = entity::ExecuteOperation(
          current_node_property.op, input1, input2,
          current_node_property.const_value.value_or(0), memory);
      dfg_id_to_output_vec_map[current_dfg_id].push_back(output_value);

      if (output_node_id == current_dfg_id) {
        output_value_vec.push_back(output_value);
      }

      searched_dfg_id_set.insert(current_dfg_id);
      for (const auto& to_dfg_id : GetAdjacentNodeIdVec(current_dfg_id)) {
        if (searched_dfg_id_set.count(to_dfg_id) == 0) {
          dfg_id_queue.push(to_dfg_id);
        }
      }
    }
  }

  return output_value_vec;
}
