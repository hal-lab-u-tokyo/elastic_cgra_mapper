#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <io/json_reader.hpp>
#include <io/mapper_config_io.hpp>

entity::MapperConfig io::ReadMapperConfigFromJsonFile(std::string file_name) {
  boost::property_tree::ptree ptree;
  boost::property_tree::read_json(file_name, ptree);

  entity::MapperConfig mapper_config;

  boost::property_tree::ptree dfg_config_ptree = ptree.get_child("DFG");
  mapper_config.dfg_config.operation_name_label =
      GetValueFromPTree<std::string>(dfg_config_ptree, "operation_name_label");

  boost::property_tree::ptree algorithm_config_ptree =
      ptree.get_child("Algorithm");
  mapper_config.algorithm_config.type =
      GetValueFromPTree<std::string>(algorithm_config_ptree, "type");
  if (auto accept_feasible_solution =
          algorithm_config_ptree.get_optional<bool>("accept_feasible_solution")) {
    mapper_config.algorithm_config.accept_feasible_solution =
        accept_feasible_solution.get();
  }
  if (auto placement_only =
          algorithm_config_ptree.get_optional<bool>("placement_only")) {
    mapper_config.algorithm_config.placement_only = placement_only.get();
  }
  if (auto max_trials = algorithm_config_ptree.get_optional<int>("max_trials")) {
    mapper_config.algorithm_config.max_trials = max_trials.get();
  }
  if (auto seed_count = algorithm_config_ptree.get_optional<int>("seed_count")) {
    mapper_config.algorithm_config.seed_count = seed_count.get();
  }
  if (auto routing_retry_count =
          algorithm_config_ptree.get_optional<int>("routing_retry_count")) {
    mapper_config.algorithm_config.routing_retry_count =
        routing_retry_count.get();
  }
  if (auto random_seed = algorithm_config_ptree.get_optional<int>("random_seed")) {
    mapper_config.algorithm_config.random_seed = random_seed.get();
  }
  if (auto max_iterations =
          algorithm_config_ptree.get_optional<int>("max_iterations")) {
    mapper_config.algorithm_config.max_iterations = max_iterations.get();
  }
  if (auto cpu_mapping_bug_compatible_degree =
          algorithm_config_ptree.get_optional<bool>(
              "cpu_mapping_bug_compatible_degree")) {
    mapper_config.algorithm_config.cpu_mapping_bug_compatible_degree =
        cpu_mapping_bug_compatible_degree.get();
  }
  if (auto io_node_policy =
          algorithm_config_ptree.get_optional<std::string>("io_node_policy")) {
    mapper_config.algorithm_config.io_node_policy = io_node_policy.get();
  }

  return mapper_config;
}

void io::WriteMapperConfigToJsonFile(
    std::string file_name, const entity::MapperConfig& mapper_config) {
  boost::property_tree::ptree dfg_config_ptree, ptree, algorithm_config_ptree;

  dfg_config_ptree.put("operation_name_label",
                       mapper_config.dfg_config.operation_name_label);

  algorithm_config_ptree.put("type", mapper_config.algorithm_config.type);
  algorithm_config_ptree.put(
      "accept_feasible_solution",
      mapper_config.algorithm_config.accept_feasible_solution);
  algorithm_config_ptree.put("placement_only",
                             mapper_config.algorithm_config.placement_only);
  if (mapper_config.algorithm_config.max_trials.has_value()) {
    algorithm_config_ptree.put("max_trials",
                               mapper_config.algorithm_config.max_trials.value());
  }
  if (mapper_config.algorithm_config.seed_count.has_value()) {
    algorithm_config_ptree.put("seed_count",
                               mapper_config.algorithm_config.seed_count.value());
  }
  if (mapper_config.algorithm_config.routing_retry_count.has_value()) {
    algorithm_config_ptree.put(
        "routing_retry_count",
        mapper_config.algorithm_config.routing_retry_count.value());
  }
  if (mapper_config.algorithm_config.random_seed.has_value()) {
    algorithm_config_ptree.put("random_seed",
                               mapper_config.algorithm_config.random_seed.value());
  }
  if (mapper_config.algorithm_config.max_iterations.has_value()) {
    algorithm_config_ptree.put(
        "max_iterations",
        mapper_config.algorithm_config.max_iterations.value());
  }
  if (mapper_config.algorithm_config.cpu_mapping_bug_compatible_degree.has_value()) {
    algorithm_config_ptree.put(
        "cpu_mapping_bug_compatible_degree",
        mapper_config.algorithm_config.cpu_mapping_bug_compatible_degree.value());
  }
  if (mapper_config.algorithm_config.io_node_policy.has_value()) {
    algorithm_config_ptree.put(
        "io_node_policy",
        mapper_config.algorithm_config.io_node_policy.value());
  }
  ptree.add_child("Algorithm", algorithm_config_ptree);
  ptree.add_child("DFG", dfg_config_ptree);

  boost::property_tree::write_json(file_name, ptree);
}
