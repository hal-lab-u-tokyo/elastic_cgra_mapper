#include <boost/foreach.hpp>
#include <boost/optional.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <io/architecture_io.hpp>
#include <io/json_reader.hpp>
#include <set>
#include <stdexcept>
#include <tuple>
#include <unordered_map>

namespace {

entity::MRRGConfig ReadMRRGConfig(
    const boost::property_tree::ptree& ptree) {
  entity::MRRGConfig mrrg_config;
  mrrg_config.column = GetValueFromPTree<int>(ptree, "column");
  mrrg_config.row = GetValueFromPTree<int>(ptree, "row");
  mrrg_config.local_reg_size = GetValueFromPTree<int>(ptree, "local_reg_size");
  mrrg_config.context_size = GetValueFromPTree<int>(ptree, "context_size");

  const auto memory_io_data =
      GetValueFromPTree<std::string>(ptree, "memory_io");
  const auto cgra_type_data =
      GetValueFromPTree<std::string>(ptree, "CGRA_type");
  const auto network_type_data =
      GetValueFromPTree<std::string>(ptree, "network_type");

  mrrg_config.memory_io =
      entity::MRRGMemoryIOTypeFromString(memory_io_data);
  mrrg_config.cgra_type = entity::MRRGCGRATypeFromString(cgra_type_data);
  mrrg_config.network_type =
      entity::MRRGNetworkTypeFromString(network_type_data);
  if (auto extension_data = ptree.get_optional<std::string>("extension")) {
    mrrg_config.extension =
        entity::MRRGExtensionTypeFromString(extension_data.get());
  }
  if (auto loop_controllers = ptree.get_child_optional("loop_controllers")) {
    BOOST_FOREACH (
        const boost::property_tree::ptree::value_type& loop_controller_data,
        *loop_controllers) {
      const int row_id =
          GetValueFromPTree<int>(loop_controller_data.second, "row_id");
      const int column_id =
          GetValueFromPTree<int>(loop_controller_data.second, "column_id");
      mrrg_config.loop_controller_position_vec.emplace_back(row_id, column_id);
    }
  }
  if (auto tm_pes = ptree.get_child_optional("tm_pes")) {
    BOOST_FOREACH (
        const boost::property_tree::ptree::value_type& tm_pe_data, *tm_pes) {
      const int row_id =
          GetValueFromPTree<int>(tm_pe_data.second, "row_id");
      const int column_id =
          GetValueFromPTree<int>(tm_pe_data.second, "column_id");
      mrrg_config.tm_pe_position_vec.emplace_back(row_id, column_id);
    }
  }

  if (mrrg_config.row <= 0 || mrrg_config.column <= 0) {
    throw std::runtime_error("MRRG row and column must be positive");
  }
  if (mrrg_config.context_size <= 0) {
    throw std::runtime_error("MRRG context_size must be positive");
  }
  if (mrrg_config.local_reg_size < 0) {
    throw std::runtime_error("MRRG local_reg_size must not be negative");
  }

  return mrrg_config;
}

void SetMRRGGraphProperty(entity::MRRGGraph& graph,
                          const entity::MRRGConfig& mrrg_config) {
  graph[boost::graph_bundle].row_num = mrrg_config.row;
  graph[boost::graph_bundle].column_num = mrrg_config.column;
  graph[boost::graph_bundle].memory_io = mrrg_config.memory_io;
  graph[boost::graph_bundle].cgra_type = mrrg_config.cgra_type;
  graph[boost::graph_bundle].network_type = mrrg_config.network_type;
  graph[boost::graph_bundle].extension = mrrg_config.extension;
  graph[boost::graph_bundle].loop_controller_position_vec =
      mrrg_config.loop_controller_position_vec;
  graph[boost::graph_bundle].tm_pe_position_vec =
      mrrg_config.tm_pe_position_vec;
}

entity::MRRG ReadExplicitMRRG(const boost::property_tree::ptree& ptree,
                              const entity::MRRGConfig& mrrg_config) {
  entity::MRRGGraph graph;
  std::unordered_map<std::string, int> external_id_to_node_id;
  std::set<std::tuple<int, int, int>> config_ids;

  const auto nodes = ptree.get_child_optional("nodes");
  if (!nodes || nodes->empty()) {
    throw std::runtime_error(
        "explicit MRRG must contain at least one node in nodes");
  }

  for (const auto& node_element : *nodes) {
    const auto& node_data = node_element.second;
    const std::string external_id =
        GetValueFromPTree<std::string>(node_data, "id");
    const int row_id = GetValueFromPTree<int>(node_data, "row_id");
    const int column_id = GetValueFromPTree<int>(node_data, "column_id");
    const int context_id = GetValueFromPTree<int>(node_data, "context_id");

    if (external_id.empty()) {
      throw std::runtime_error("explicit MRRG node id must not be empty");
    }
    if (external_id_to_node_id.count(external_id) > 0) {
      throw std::runtime_error("duplicate explicit MRRG node id: " +
                               external_id);
    }
    if (context_id < 0 || context_id >= mrrg_config.context_size) {
      throw std::runtime_error("context_id is out of range for node: " +
                               external_id);
    }

    const auto config_id = std::make_tuple(row_id, column_id, context_id);
    if (!config_ids.emplace(config_id).second) {
      throw std::runtime_error(
          "duplicate (row_id, column_id, context_id) in explicit MRRG node: " +
          external_id);
    }

    const int local_reg_size =
        node_data.get<int>("local_reg_size", mrrg_config.local_reg_size);
    if (local_reg_size < 0) {
      throw std::runtime_error("local_reg_size must not be negative for node: " +
                               external_id);
    }

    const auto supported_operations =
        node_data.get_child_optional("supported_operations");
    if (!supported_operations) {
      throw std::runtime_error(
          "explicit MRRG node must contain supported_operations: " +
          external_id);
    }

    auto vertex_id = boost::add_vertex(graph);
    graph[vertex_id].position_id = {row_id, column_id};
    graph[vertex_id].context_id = context_id;
    graph[vertex_id].is_memory_accessible =
        GetValueFromPTree<bool>(node_data, "memory_accessible");
    graph[vertex_id].local_reg_size = local_reg_size;
    graph[vertex_id].context_size = mrrg_config.context_size;
    for (const auto& operation_element : *supported_operations) {
      graph[vertex_id].supported_operations.emplace_back(
          entity::OpTypeFromString(
              operation_element.second.get_value<std::string>()));
    }

    external_id_to_node_id.emplace(external_id,
                                   static_cast<int>(vertex_id));
  }

  std::set<std::pair<int, int>> explicit_edges;
  if (const auto edges = ptree.get_child_optional("edges")) {
    for (const auto& edge_element : *edges) {
      const auto& edge_data = edge_element.second;
      const std::string from =
          GetValueFromPTree<std::string>(edge_data, "from");
      const std::string to = GetValueFromPTree<std::string>(edge_data, "to");

      if (external_id_to_node_id.count(from) == 0) {
        throw std::runtime_error("explicit MRRG edge refers to unknown from: " +
                                 from);
      }
      if (external_id_to_node_id.count(to) == 0) {
        throw std::runtime_error("explicit MRRG edge refers to unknown to: " +
                                 to);
      }

      const int from_node_id = external_id_to_node_id.at(from);
      const int to_node_id = external_id_to_node_id.at(to);
      if (!explicit_edges.emplace(from_node_id, to_node_id).second) {
        throw std::runtime_error("duplicate explicit MRRG edge: " + from +
                                 " -> " + to);
      }
      boost::add_edge(from_node_id, to_node_id, graph);
    }
  }

  SetMRRGGraphProperty(graph, mrrg_config);
  return entity::MRRG(graph);
}

}  // namespace

entity::MRRG io::ReadMRRGFromJsonFile(std::string file_name) {
  boost::property_tree::ptree ptree;
  boost::property_tree::read_json(file_name, ptree);

  const entity::MRRGConfig mrrg_config = ReadMRRGConfig(ptree);
  const std::string format = ptree.get<std::string>("format", "generated");
  if (format == "explicit") {
    return ReadExplicitMRRG(ptree, mrrg_config);
  }
  if (format != "generated") {
    throw std::runtime_error("unknown MRRG JSON format: " + format);
  }

  return entity::MRRG(mrrg_config);
}

void io::WriteMRRGToJsonFile(std::string file_name,
                             std::shared_ptr<entity::MRRG> mrrg_ptr_) {
  boost::property_tree::ptree ptree;
  entity::MRRGConfig mrrg_config = mrrg_ptr_->GetMRRGConfig();

  ptree.put("column", mrrg_config.column);
  ptree.put("row", mrrg_config.row);
  ptree.put("memory_io",
            entity::MRRGMemoryIoTypeToString(mrrg_config.memory_io));
  ptree.put("CGRA_type", entity::MRRGCGRATypeToString(mrrg_config.cgra_type));
  ptree.put("network_type",
            entity::MRRGNetworkTypeToString(mrrg_config.network_type));
  if (mrrg_config.extension != entity::MRRGExtensionType::kNone) {
    ptree.put("extension",
              entity::MRRGExtensionTypeToString(mrrg_config.extension));
  }
  ptree.put("local_reg_size", mrrg_config.local_reg_size);
  ptree.put("context_size", mrrg_config.context_size);
  boost::property_tree::ptree loop_controllers;
  for (auto loop_controller_position :
       mrrg_config.loop_controller_position_vec) {
    boost::property_tree::ptree loop_controller_node;
    loop_controller_node.put("row_id", loop_controller_position.row_id);
    loop_controller_node.put("column_id", loop_controller_position.column_id);
    loop_controllers.push_back(std::make_pair("", loop_controller_node));
  }
  ptree.add_child("loop_controllers", loop_controllers);
  boost::property_tree::ptree tm_pes;
  for (auto tm_pe_position : mrrg_config.tm_pe_position_vec) {
    boost::property_tree::ptree tm_pe_node;
    tm_pe_node.put("row_id", tm_pe_position.row_id);
    tm_pe_node.put("column_id", tm_pe_position.column_id);
    tm_pes.push_back(std::make_pair("", tm_pe_node));
  }
  ptree.add_child("tm_pes", tm_pes);

  boost::property_tree::write_json(file_name, ptree);
  return;
}
