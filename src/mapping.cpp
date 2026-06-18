#include <time.h>

#include <chrono>
#include <cxxopts.hpp>
#include <filesystem>
#include <fstream>
#include <io/architecture_io.hpp>
#include <io/dfg_io.hpp>
#include <io/mapper_config_io.hpp>
#include <io/mapping_io.hpp>
#include <io/output_to_log_file.hpp>
#include <iostream>
#include <mapper/gurobi_mapper.hpp>
#include <mapper/gurobi_placement_mapper.hpp>
#include <mapper/mapper.hpp>

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
  cxxopts::Options options("mapping", "Run mapping for a DFG and CGRA.");
  options.add_options()("dfg_file", "Absolute path to the input DFG dot file",
                        cxxopts::value<std::string>())(
      "cgra_file", "Absolute path to the input CGRA/MRRG json file",
      cxxopts::value<std::string>())("output_dir",
                                     "Absolute path to the output directory",
                                     cxxopts::value<std::string>())(
      "mapper_config", "Path to the mapper config json file",
      cxxopts::value<std::string>())("timeout_s", "Mapping timeout in seconds",
                                     cxxopts::value<double>())(
      "parallel_num", "Number of parallel DFG instances",
      cxxopts::value<int>())("h,help", "Print usage");

  std::string dfg_dot_file_path;
  std::string mrrg_file_path;
  std::string output_dir;
  std::string mapper_config_file_path;
  double timeout_s = 0;
  int parallel_num = 0;
  try {
    const auto result = options.parse(argc, argv);
    if (result.count("help")) {
      std::cout << options.help();
      return 0;
    }
    dfg_dot_file_path = result["dfg_file"].as<std::string>();
    mrrg_file_path = result["cgra_file"].as<std::string>();
    output_dir = result["output_dir"].as<std::string>();
    mapper_config_file_path = result["mapper_config"].as<std::string>();
    timeout_s = result["timeout_s"].as<double>();
    parallel_num = result["parallel_num"].as<int>();
  } catch (const cxxopts::exceptions::exception& e) {
    std::cerr << "invalid arguments: " << e.what() << std::endl;
    std::cerr << options.help();
    return 1;
  }

  io::MappingLogger logger;

  if (!std::filesystem::exists(output_dir)) {
    std::filesystem::create_directories(output_dir);
  };

  assert(std::filesystem::path(dfg_dot_file_path).is_absolute());
  assert(std::filesystem::path(mrrg_file_path).is_absolute());
  assert(std::filesystem::path(output_dir).is_absolute());
  assert(std::filesystem::exists(dfg_dot_file_path));
  assert(std::filesystem::exists(mrrg_file_path));

  std::shared_ptr<entity::DFG> dfg_ptr = std::make_shared<entity::DFG>();
  std::shared_ptr<entity::DFG> dfg_ptr_to_add = std::make_shared<entity::DFG>();
  std::shared_ptr<entity::MRRG> mrrg_ptr = std::make_shared<entity::MRRG>();

  entity::MapperConfig mapper_config =
      io::ReadMapperConfigFromJsonFile(mapper_config_file_path);
  *dfg_ptr_to_add =
      io::ReadDFGDotFile(dfg_dot_file_path, mapper_config.dfg_config);
  *dfg_ptr = *dfg_ptr_to_add;
  *mrrg_ptr = io::ReadMRRGFromJsonFile(mrrg_file_path);

  io::MappingInput input;
  input.dfg_dot_file_path = dfg_dot_file_path;
  input.mrrg_config = mrrg_ptr->GetMRRGConfig();
  input.output_dir_path = output_dir;
  input.timeout_s = timeout_s;
  input.parallel_num = parallel_num;
  logger.LogMappingInput(input);

  for (int i = 1; i < parallel_num; i++) {
    dfg_ptr = AddDFG(dfg_ptr, dfg_ptr_to_add, dfg_ptr_to_add->GetNodeNum() * i);
  }

  mapper::IILPMapper* mapper_impl;
  if (mapper_config.algorithm_config.algorithm ==
      entity::AlgorithmType::kILPMapper) {
    mapper_impl = mapper::GurobiILPMapper().CreateMapper(dfg_ptr, mrrg_ptr);
  } else if (mapper_config.algorithm_config.algorithm ==
             entity::AlgorithmType::kPlacementILPMapper) {
    mapper_impl =
        mapper::GurobiPlacementILPMapper().CreateMapper(dfg_ptr, mrrg_ptr);
  } else {
    std::cerr << "Invalid algorithm type in mapper config: "
              << static_cast<int>(mapper_config.algorithm_config.algorithm)
              << std::endl;
    abort();
  }
  mapper_impl->SetLogFilePath(logger.GetGurobiLogFilePath());
  mapper_impl->SetTimeOut(timeout_s);

  const auto result = mapper_impl->Execution();

  io::MappingOutput output;
  output.mapping_time_s = result.mapping_time_s;
  output.is_success = result.is_success;
  output.mapping_ptr = result.mapping_ptr;
  output.mrrg_config = mrrg_ptr->GetMRRGConfig();
  logger.LogMappingOutput(output);
}
