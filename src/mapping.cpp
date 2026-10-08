#include <time.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <io/architecture_io.hpp>
#include <io/dfg_io.hpp>
#include <io/mapper_config_io.hpp>
#include <io/mapping_io.hpp>
#include <io/output_to_log_file.hpp>
#include <iostream>
#include <mapper/mapper_factory.hpp>

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
  if (argc != 7) {
    std::cerr << "invalid arguments" << std::endl;
    abort();
  }

  io::MappingLogger logger;

  std::string dfg_dot_file_path = argv[1];
  std::string mrrg_file_path = argv[2];
  std::string output_dir = argv[3];
  std::string mapper_config_file_path = argv[4];
  double timeout_s = std::stod(argv[5]);
  int parallel_num = std::stoi(argv[6]);

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

  std::unique_ptr<mapper::IMapper> mapper_impl = mapper::CreateMapper(
      mapper_config.algorithm_config.type, dfg_ptr, mrrg_ptr);
  mapper::MapperOptions options;
  options.log_file_path = logger.GetGurobiLogFilePath();
  options.timeout_s = timeout_s;
  options.accept_feasible_solution =
      mapper_config.algorithm_config.accept_feasible_solution;
  options.placement_only = mapper_config.algorithm_config.placement_only;
  options.max_trials = mapper_config.algorithm_config.max_trials;
  options.seed_count = mapper_config.algorithm_config.seed_count;
  options.routing_retry_count =
      mapper_config.algorithm_config.routing_retry_count;
  options.random_seed = mapper_config.algorithm_config.random_seed;
  options.max_iterations = mapper_config.algorithm_config.max_iterations;
  options.elite_placement_count =
      mapper_config.algorithm_config.elite_placement_count;
  options.io_node_policy = mapper_config.algorithm_config.io_node_policy;
  options.trial_seed_policy = mapper_config.algorithm_config.trial_seed_policy;
  options.traversal_order_policy =
      mapper_config.algorithm_config.traversal_order_policy;
  options.traversal_neighbor_policy =
      mapper_config.algorithm_config.traversal_neighbor_policy;
  options.candidate_scope_policy =
      mapper_config.algorithm_config.candidate_scope_policy;
  options.candidate_rank_policy =
      mapper_config.algorithm_config.candidate_rank_policy;
  options.use_yott_annotations =
      mapper_config.algorithm_config.use_yott_annotations;
  options.trace_trials = mapper_config.algorithm_config.trace_trials;
  mapper_impl->Configure(options);

  const auto result = mapper_impl->Execution();

  io::MappingOutput output;
  output.mapping_time_s = result.mapping_time_s;
  output.is_success = result.is_success;
  output.mapping_ptr = result.mapping_ptr;
  output.mrrg_config = mrrg_ptr->GetMRRGConfig();
  logger.LogMappingOutput(output);
}
