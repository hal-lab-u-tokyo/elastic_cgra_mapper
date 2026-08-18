#include <entity/mrrg.hpp>
#include <cassert>
#include <cstdlib>
#include <map>
#include <stdexcept>

bool entity::MRRGConfig::IsLoopController(
    entity::PEPositionId position_id) const {
  for (auto loop_controller_position : loop_controller_position_vec) {
    if (position_id == loop_controller_position) {
      return true;
    }
  }
  return false;
}

bool entity::MRRGConfig::IsTMPE(
    entity::PEPositionId position_id) const {
  for (auto tm_pe_position : tm_pe_position_vec) {
    if (position_id == tm_pe_position) {
      return true;
    }
  }
  return false;
}

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
  } else if (memory_io_type_string == "one_end") {
    return entity::MRRGMemoryIOType::kOneEnd;
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
    case entity::MRRGMemoryIOType::kOneEnd:
      return "one_end";
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

entity::MRRGExtensionType entity::MRRGExtensionTypeFromString(
    std::string extension_type_string) {
  if (extension_type_string == "none") {
    return entity::MRRGExtensionType::kNone;
  } else if (extension_type_string == "outer_lspe_group4") {
    return entity::MRRGExtensionType::kOuterLspeGroup4;
  } else {
    assert(false && "invalid MRRG Extension Type String");
    abort();
  }
};

std::string entity::MRRGExtensionTypeToString(
    entity::MRRGExtensionType extension_type) {
  switch (extension_type) {
    case entity::MRRGExtensionType::kNone:
      return "none";
      break;
    case entity::MRRGExtensionType::kOuterLspeGroup4:
      return "outer_lspe_group4";
      break;
    default:
      assert(false && "invalid MRRG Extension Type");
      abort();
  }
};

entity::MRRG::MRRG(entity::MRRGGraph mrrg_graph)
    : entity::BaseGraphClass<entity::MRRGNodeProperty, entity::MRRGEdgeProperty,
                             entity::MRRGGraphProperty>(mrrg_graph),
      config_id_to_node_id_map_({}) {
  for (int node_id = 0; node_id < GetNodeNum(); ++node_id) {
    const auto node_property = GetNodeProperty(node_id);
    const auto config_id =
        std::make_tuple(node_property.position_id.first,
                        node_property.position_id.second,
                        node_property.context_id);
    if (!config_id_to_node_id_map_.emplace(config_id, node_id).second) {
      throw std::invalid_argument(
          "MRRG nodes must have unique (row, column, context) identifiers");
    }
  }
};

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

void AddOuterLspeGroup4Extension(
    entity::MRRGGraph& graph,
    std::map<std::tuple<int, int, int>, int>& node_id_to_vertex_id,
    const entity::MRRGConfig& mrrg_config) {
  if (mrrg_config.row != 16 || mrrg_config.column != 16) {
    assert(false &&
           "outer_lspe_group4 extension is currently only for 16x16 CGRA");
    abort();
  }
  if (mrrg_config.memory_io != entity::MRRGMemoryIOType::kBothEnds) {
    assert(false &&
           "outer_lspe_group4 extension requires both_ends memory_io");
    abort();
  }

  constexpr int kGroupSize = 4;
  constexpr int kLeftOuterColumnId = -1;
  const int right_outer_column_id = mrrg_config.column;
  const int left_lspe_column_id = 0;
  const int right_lspe_column_id = mrrg_config.column - 1;

  auto add_outer_nodes = [&](int outer_column_id) {
    for (int row_group_start = 0; row_group_start < mrrg_config.row;
         row_group_start += kGroupSize) {
      for (int context_id = 0; context_id < mrrg_config.context_size;
           context_id++) {
        auto vertex_id = boost::add_vertex(graph);
        graph[vertex_id].position_id =
            std::pair<int, int>(row_group_start, outer_column_id);
        graph[vertex_id].context_id = context_id;
        graph[vertex_id].is_memory_accessible = false;
        graph[vertex_id].local_reg_size = mrrg_config.local_reg_size;
        graph[vertex_id].context_size = mrrg_config.context_size;
        graph[vertex_id].supported_operations =
            entity::GetOuterLspeOperations();

        node_id_to_vertex_id[{row_group_start, outer_column_id, context_id}] =
            vertex_id;
      }
    }
  };

  add_outer_nodes(kLeftOuterColumnId);
  add_outer_nodes(right_outer_column_id);

  auto add_timed_edges = [&](std::tuple<int, int, int> from_node_id, int to_row,
                             int to_column) {
    int from_context = std::get<2>(from_node_id);

    if (mrrg_config.cgra_type == entity::MRRGCGRAType::kDefault) {
      std::tuple<int, int, int> to_node_id(
          to_row, to_column, (from_context + 1) % mrrg_config.context_size);
      boost::add_edge(node_id_to_vertex_id[from_node_id],
                      node_id_to_vertex_id[to_node_id], graph);
    } else if (mrrg_config.cgra_type == entity::MRRGCGRAType::kElastic) {
      for (int to_context = 0; to_context < mrrg_config.context_size;
           to_context++) {
        std::tuple<int, int, int> to_node_id(to_row, to_column, to_context);
        boost::add_edge(node_id_to_vertex_id[from_node_id],
                        node_id_to_vertex_id[to_node_id], graph);
      }
    }
  };

  auto connect_outer_side = [&](int outer_column_id, int lspe_column_id) {
    for (int row_group_start = 0; row_group_start < mrrg_config.row;
         row_group_start += kGroupSize) {
      for (int context_id = 0; context_id < mrrg_config.context_size;
           context_id++) {
        std::tuple<int, int, int> outer_node_id(
            row_group_start, outer_column_id, context_id);
        for (int row_id = row_group_start;
             row_id < row_group_start + kGroupSize; row_id++) {
          std::tuple<int, int, int> lspe_node_id(row_id, lspe_column_id,
                                                 context_id);
          add_timed_edges(outer_node_id, row_id, lspe_column_id);
          add_timed_edges(lspe_node_id, row_group_start, outer_column_id);
        }
      }
    }
  };

  connect_outer_side(kLeftOuterColumnId, left_lspe_column_id);
  connect_outer_side(right_outer_column_id, right_lspe_column_id);
}

std::vector<entity::OpType> GetSupportedOperation(
    entity::PEPositionId position_id, entity::MRRGConfig& mrrg_config) {
  // Implementation for getting supported operations based on position and
  // config
  const bool is_loop_controller = mrrg_config.IsLoopController(position_id);
  const bool is_tm_pe = mrrg_config.IsTMPE(position_id);

  if (is_loop_controller && is_tm_pe) {
    return std::vector<entity::OpType>(
        {entity::OpType::LOOP, entity::OpType::TM});
  }
  if (is_loop_controller) {
    return entity::GetLoopOperations();
  }
  if (is_tm_pe) {
    return entity::GetTMOperations();
  }

  if (mrrg_config.memory_io == entity::MRRGMemoryIOType::kAll) {
    return entity::GetAllOperations();
  } else if (mrrg_config.memory_io == entity::MRRGMemoryIOType::kBothEnds) {
    if (position_id.column_id == 0 ||
        position_id.column_id == mrrg_config.column - 1) {
      return entity::GetAllOperations();
    } else {
      return entity::GetAllOperationsExceptMemoryAccess();
    }
  } else if (mrrg_config.memory_io == entity::MRRGMemoryIOType::kOneEnd) {
    if (position_id.column_id == 0) {
      return entity::GetAllOperations();
    } else {
      return entity::GetAllOperationsExceptMemoryAccess();
    }
  }
}

entity::MRRG::MRRG(entity::MRRGConfig mrrg_config)
    : entity::BaseGraphClass<entity::MRRGNodeProperty, entity::MRRGEdgeProperty,
                             entity::MRRGGraphProperty>(),
      config_id_to_node_id_map_({}) {
  entity::MRRGGraph mrrg_graph;
  std::map<std::tuple<int, int, int>, int> node_id_to_vertex_id;

  for (int i = 0; i < mrrg_config.row; i++) {
    for (int j = 0; j < mrrg_config.column; j++) {
      std::vector<entity::OpType> supported_operations =
          GetSupportedOperation({i, j}, mrrg_config);

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
        graph_[vertex_id].supported_operations = supported_operations;

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

  if (mrrg_config.extension == entity::MRRGExtensionType::kOuterLspeGroup4) {
    AddOuterLspeGroup4Extension(graph_, node_id_to_vertex_id, mrrg_config);
  }
  for (const auto& node_id_and_vertex_id : node_id_to_vertex_id) {
    config_id_to_node_id_map_.emplace(node_id_and_vertex_id.first,
                                      node_id_and_vertex_id.second);
  }

  graph_[boost::graph_bundle].row_num = mrrg_config.row;
  graph_[boost::graph_bundle].column_num = mrrg_config.column;
  graph_[boost::graph_bundle].memory_io = mrrg_config.memory_io;
  graph_[boost::graph_bundle].cgra_type = mrrg_config.cgra_type;
  graph_[boost::graph_bundle].network_type = mrrg_config.network_type;
  graph_[boost::graph_bundle].extension = mrrg_config.extension;
  graph_[boost::graph_bundle].loop_controller_position_vec =
      mrrg_config.loop_controller_position_vec;
  graph_[boost::graph_bundle].tm_pe_position_vec =
      mrrg_config.tm_pe_position_vec;
};

entity::MRRGConfig entity::MRRG::GetMRRGConfig() const {
  entity::MRRGConfig mrrg_config;
  mrrg_config.row = graph_[boost::graph_bundle].row_num;
  mrrg_config.column = graph_[boost::graph_bundle].column_num;
  mrrg_config.memory_io = graph_[boost::graph_bundle].memory_io;
  mrrg_config.cgra_type = graph_[boost::graph_bundle].cgra_type;
  mrrg_config.network_type = graph_[boost::graph_bundle].network_type;
  mrrg_config.extension = graph_[boost::graph_bundle].extension;
  mrrg_config.local_reg_size = graph_[0].local_reg_size;
  mrrg_config.context_size = graph_[0].context_size;
  mrrg_config.loop_controller_position_vec =
      graph_[boost::graph_bundle].loop_controller_position_vec;
  mrrg_config.tm_pe_position_vec =
      graph_[boost::graph_bundle].tm_pe_position_vec;

  return mrrg_config;
}

int entity::MRRG::GetMRRGNodeId(int row_id, int column_id, int context_id) {
  std::tuple<int, int, int> config_id(row_id, column_id, context_id);
  if (config_id_to_node_id_map_.count(config_id) == 0) return -1;
  return config_id_to_node_id_map_[config_id];
}
