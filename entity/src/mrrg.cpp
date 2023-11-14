#include <entity/mrrg.hpp>
#include <map>

entity::MRRGCGRAType entity::MRRGCGRATypeFromString(
    std::string cgra_type_string) {
  if (cgra_type_string == "default") {
    return entity::MRRGCGRAType::kDefault;
  } else if (cgra_type_string == "elastic") {
    return entity::MRRGCGRAType::kElastic;
  } else {
    assert("invalid MRRG CGRA Type");
    abort();
  }
};

std::string entity::MRRGCGRATypeToString(entity::MRRGCGRAType cgra_type) {
  switch (cgra_type) {
    case entity::MRRGCGRAType::kDefault:
      return "default";
      break;
    case entity::MRRGCGRAType::kElastic:
      return "elastic";
      break;
    default:
      assert("invalid MRRG CGRA Type String");
      abort();
  }
};

entity::MRRGMemoryIOType entity::MRRGMemoryIOTypeFromString(
    std::string memory_io_type_string) {
  if (memory_io_type_string == "all") {
    return entity::MRRGMemoryIOType::kAll;
  } else if (memory_io_type_string == "both_ends") {
    return entity::MRRGMemoryIOType::kBothEnds;
  } else {
    assert("invalid Memory IO Type String");
    abort();
  }
};

std::string entity::MRRGMemoryIoTypeToString(
    entity::MRRGMemoryIOType memory_io_type) {
  switch (memory_io_type) {
    case entity::MRRGMemoryIOType::kAll:
      return "all";
      break;
    case entity::MRRGMemoryIOType::kBothEnds:
      return "both_ends";
      break;
    default:
      assert("invalid MRRG Memory IO Type");
      abort();
  }
};

entity::MRRGNetworkType entity::MRRGNetworkTypeFromString(
    std::string network_type_string) {
  if (network_type_string == "orthogonal") {
    return entity::MRRGNetworkType::kOrthogonal;
  } else if (network_type_string == "diagonal") {
    return entity::MRRGNetworkType::kDiagonal;
  } else {
    assert("invalid Network Type String");
    abort();
  }
};

std::string entity::MRRGNetworkTypeToString(
    entity::MRRGNetworkType network_type) {
  switch (network_type) {
    case entity::MRRGNetworkType::kOrthogonal:
      return "orthogonal";
      break;
    case entity::MRRGNetworkType::kDiagonal:
      return "diagonal";
      break;
    default:
      assert("invalid Network Type");
      abort();
  }
};

entity::MRRG::MRRG(entity::MRRGGraph mrrg_graph)
    : entity::BaseGraphClass<entity::MRRGNodeProperty, entity::MRRGEdgeProperty,
                             entity::MRRGGraphProperty>(mrrg_graph),
      config_id_to_node_id_map_({}){};

std::vector<std::tuple<int, int, int>> GetConnectedNodeIdVector(
    std::tuple<int, int, int> from_node_id,
    const entity::MRRGConfig& mrrg_config) {
  std::vector<std::tuple<int, int, int>> result;
  std::vector<std::tuple<int, int>> spatial_connected_node_vec;
  int from_row_id, from_column_id, from_context_id;
  std::tie(from_row_id, from_column_id, from_context_id) = from_node_id;

  auto is_available = [&mrrg_config](int row, int column) {
    if (row < 0 || column < 0) return false;
    if (row >= mrrg_config.row || column >= mrrg_config.column) return false;
    return true;
  };

  for (int i = -1; i <= 1; i++) {
    for (int j = -1; j <= 1; j++) {
      if (i == 0 && j == 0) continue;
      if (mrrg_config.network_type == entity::MRRGNetworkType::kOrthogonal &&
          abs(i) + abs(j) > 1) {
        continue;
      }

      if (is_available(from_row_id + i, from_column_id + j)) {
        spatial_connected_node_vec.emplace_back(from_row_id + i,
                                                from_column_id + j);
      }
    }
  }

  for (auto spatial_connected_node : spatial_connected_node_vec) {
    int to_row_id, to_column_id;
    std::tie(to_row_id, to_column_id) = spatial_connected_node;

    if (mrrg_config.cgra_type == entity::MRRGCGRAType::kDefault) {
      result.emplace_back(to_row_id, to_column_id,
                          (from_context_id + 1) % mrrg_config.context_size);
    } else if (mrrg_config.cgra_type == entity::MRRGCGRAType::kElastic) {
      for (int i = 0; i < mrrg_config.context_size; i++) {
        result.emplace_back(to_row_id, to_column_id, i);
      }
    }
  }

  result.emplace_back(from_row_id, from_column_id,
                      (from_context_id + 1) % mrrg_config.context_size);

  return result;
};

entity::MRRG::MRRG(entity::MRRGConfig mrrg_config)
    : entity::BaseGraphClass<entity::MRRGNodeProperty, entity::MRRGEdgeProperty,
                             entity::MRRGGraphProperty>(),
      config_id_to_node_id_map_({}) {
  entity::MRRGGraph mrrg_graph;
  std::map<std::tuple<int, int, int>, int> node_id_to_vertex_id;

  for (int i = 0; i < mrrg_config.row; i++) {
    for (int j = 0; j < mrrg_config.column; j++) {
      for (int k = 0; k < mrrg_config.context_size; k++) {
        // add node, (with memory_io and local reg size property)
        auto vertex_id = boost::add_vertex(graph_);
        graph_[vertex_id].position_id = std::pair<int, int>(i, j);
        graph_[vertex_id].context_id = k;
        node_id_to_vertex_id[{i, j, k}] = vertex_id;

        if (mrrg_config.memory_io == entity::MRRGMemoryIOType::kAll) {
          graph_[vertex_id].is_memory_accessible = true;
        }
        graph_[vertex_id].local_reg_size = mrrg_config.local_reg_size;
        graph_[vertex_id].context_size = mrrg_config.context_size;

        if (mrrg_config.memory_io == entity::MRRGMemoryIOType::kAll) {
          graph_[vertex_id].supported_operations = entity::GetAllOperations();
        } else if (mrrg_config.memory_io ==
                   entity::MRRGMemoryIOType::kBothEnds) {
          if (j == 0 || j == mrrg_config.column - 1) {
            graph_[vertex_id].supported_operations = entity::GetAllOperations();
          } else {
            graph_[vertex_id].supported_operations =
                entity::GetAllOperationsExceptMemoryAccess();
          }
        }

        std::tuple<int, int, int> config_id(i, j, k);
        config_id_to_node_id_map_.emplace(config_id, vertex_id);
      }
    }
  }

  // add edge (with elastic, network property)
  for (int i = 0; i < mrrg_config.row; i++) {
    for (int j = 0; j < mrrg_config.column; j++) {
      for (int k = 0; k < mrrg_config.context_size; k++) {
        std::tuple<int, int, int> from_node_id({i, j, k});
        auto connected_node_id_vec =
            GetConnectedNodeIdVector(from_node_id, mrrg_config);
        int from_vertex_id = node_id_to_vertex_id[from_node_id];
        for (auto to_node_id : connected_node_id_vec) {
          int to_vertex_id = node_id_to_vertex_id[to_node_id];
          boost::add_edge(from_vertex_id, to_vertex_id, graph_);
        }
      }
    }
  }

  graph_[boost::graph_bundle].row_num = mrrg_config.row;
  graph_[boost::graph_bundle].column_num = mrrg_config.column;
  graph_[boost::graph_bundle].memory_io = mrrg_config.memory_io;
  graph_[boost::graph_bundle].cgra_type = mrrg_config.cgra_type;
  graph_[boost::graph_bundle].network_type = mrrg_config.network_type;
};

entity::MRRGConfig entity::MRRG::GetMRRGConfig() const {
  entity::MRRGConfig mrrg_config;
  mrrg_config.row = graph_[boost::graph_bundle].row_num;
  mrrg_config.column = graph_[boost::graph_bundle].column_num;
  mrrg_config.memory_io = graph_[boost::graph_bundle].memory_io;
  mrrg_config.cgra_type = graph_[boost::graph_bundle].cgra_type;
  mrrg_config.network_type = graph_[boost::graph_bundle].network_type;
  mrrg_config.local_reg_size = graph_[0].local_reg_size;
  mrrg_config.context_size = graph_[0].context_size;

  return mrrg_config;
}

int entity::MRRG::GetMRRGNodeId(int row_id, int column_id, int context_id) {
  std::tuple<int, int, int> config_id(row_id, column_id, context_id);
  if (config_id_to_node_id_map_.count(config_id) == 0) return -1;
  return config_id_to_node_id_map_[config_id];
}