#include <time.h>

#include <chrono>
#include <entity/architecture.hpp>
#include <filesystem>
#include <io/dfg_io.hpp>
#include <io/mapping_io.hpp>
#include <mapper/gurobi_mapper.hpp>
#include <string>

std::string FixOpName(std::string op_name, int node_offset) {
  std::string result = "";
  std::string number_str = "";

  const auto AddNumber = [&]() {
    if (number_str != "") {
      int number = std::stoi(number_str);
      number += node_offset;
      result += std::to_string(number);
      number_str = "";
    }
  };

  for (const auto c : op_name) {
    if (c >= '0' && c <= '9') {
      number_str += c;
    } else {
      AddNumber();
      result += c;
    }
  }
  AddNumber();
  return result;
};

std::shared_ptr<entity::DFG> AddDFG(
    std::shared_ptr<entity::DFG> original_dfg_ptr,
    std::shared_ptr<entity::DFG> dfg_ptr_to_add, int node_offset) {
  entity::DFGGraph g_to_add = dfg_ptr_to_add->GetGraph();
  entity::DFGGraph result_graph = original_dfg_ptr->GetGraph();

  std::unordered_map<int, size_t> dfg_id_map = {};
  for (int i = 0; i < dfg_ptr_to_add->GetNodeNum(); i++) {
    auto tmp_node_property = dfg_ptr_to_add->GetNodeProperty(i);
    auto v = boost::add_vertex(result_graph);
    result_graph[v].op = tmp_node_property.op;
    result_graph[v].op_name = FixOpName(tmp_node_property.op_name, node_offset);
    result_graph[v].op_str = tmp_node_property.op_str;
    result_graph[v].const_value = tmp_node_property.const_value;

    dfg_id_map[i] = v;
  }

  entity::DFGGraph::edge_iterator ei, ei_end;
  bool found;
  for (std::tie(ei, ei_end) = boost::edges(g_to_add); ei != ei_end; ++ei) {
    entity::DFGGraph::edge_descriptor e = *ei;
    entity::DFGGraph::vertex_descriptor u = boost::source(e, g_to_add),
                                        v = boost::target(e, g_to_add);
    entity::DFGGraph::vertex_descriptor new_u = dfg_id_map[u],
                                        new_v = dfg_id_map[v];

    boost::tie(e, found) = boost::add_edge(new_u, new_v, result_graph);
    result_graph[e].operand = dfg_ptr_to_add->GetEdgeProperty(e).operand;
  }

  std::shared_ptr<entity::DFG> result_dfg_ptr =
      std::make_shared<entity::DFG>(entity::DFG(result_graph));
  return result_dfg_ptr;
}

int main(int argc, char* argv[]) {
  const std::string dfg_dot_file_path = argv[1];
  const std::string output_mapping_dir = argv[2];
  const std::string log_file_dir = argv[3];

  if (!std::filesystem::exists(output_mapping_dir)) {
    std::filesystem::create_directories(output_mapping_dir);
  };
  if (!std::filesystem::exists(log_file_dir)) {
    std::filesystem::create_directories(log_file_dir);
  }

  // set param
  const int cgra_row = 20;
  const int cgra_column = 20;

  std::shared_ptr<entity::DFG> dfg_ptr_to_add = std::make_shared<entity::DFG>();
  std::shared_ptr<entity::DFG> dfg_ptr = std::make_shared<entity::DFG>();
  *dfg_ptr_to_add = io::ReadDFGDotFile(dfg_dot_file_path);
  *dfg_ptr = *dfg_ptr_to_add;

  entity::MRRGConfig mrrg_config;
  mrrg_config.cgra_type = entity::MRRGCGRAType::kDefault;
  mrrg_config.memory_io = entity::MRRGMemoryIOType::kBothEnds;
  mrrg_config.network_type = entity::MRRGNetworkType::kOrthogonal;
  mrrg_config.context_size = 1;
  mrrg_config.row = cgra_row;
  mrrg_config.column = cgra_column;
  mrrg_config.local_reg_size = 1;

  for (int parallel_num = 1; parallel_num <= 10; parallel_num++) {
    io::WriteDFGDotFile(
        log_file_dir + "dfg_" + std::to_string(parallel_num) + ".dot", dfg_ptr);
    for (int memory_io_type = 0; memory_io_type < 2; memory_io_type++) {
      mrrg_config.context_size = 1;
      mrrg_config.memory_io =
          static_cast<entity::MRRGMemoryIOType>(memory_io_type);
      bool is_success = false;

      while (!is_success) {
        std::shared_ptr<entity::MRRG> mrrg_ptr =
            std::make_shared<entity::MRRG>(mrrg_config);
        const auto tmp_time = std::time(0);
        std::string log_file_path =
            log_file_dir + "log" + std::to_string(tmp_time) + ".log";
        mapper::GurobiILPMapper* mapper;
        mapper = mapper::GurobiILPMapper().CreateMapper(dfg_ptr, mrrg_ptr);
        mapper->SetLogFilePath(log_file_path);

        std::shared_ptr<entity::Mapping> mapping_ptr =
            std::make_shared<entity::Mapping>();
        std::tie(is_success, *mapping_ptr) = mapper->Execution();

        std::string output_mapping_path = output_mapping_dir + "mapping_" +
                                          std::to_string(tmp_time) + ".json";
        if (is_success) {
          io::WriteMappingFile(output_mapping_path, mapping_ptr,
                               mrrg_ptr->GetMRRGConfig());
        } else {
          mrrg_config.context_size++;
        }
      }
    }
    dfg_ptr = AddDFG(dfg_ptr, dfg_ptr_to_add,
                     dfg_ptr_to_add->GetNodeNum() * parallel_num);
  }
}