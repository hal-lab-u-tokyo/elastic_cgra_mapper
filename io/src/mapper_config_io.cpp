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
  if (auto elite_placement_count =
          algorithm_config_ptree.get_optional<int>("elite_placement_count")) {
    mapper_config.algorithm_config.elite_placement_count =
        elite_placement_count.get();
  }
  if (auto io_node_policy =
          algorithm_config_ptree.get_optional<std::string>("io_node_policy")) {
    mapper_config.algorithm_config.io_node_policy = io_node_policy.get();
  }
  if (auto trial_seed_policy =
          algorithm_config_ptree.get_optional<std::string>(
              "trial_seed_policy")) {
    mapper_config.algorithm_config.trial_seed_policy =
        trial_seed_policy.get();
  }
  if (auto traversal_order_policy =
          algorithm_config_ptree.get_optional<std::string>(
              "traversal_order_policy")) {
    mapper_config.algorithm_config.traversal_order_policy =
        traversal_order_policy.get();
  }
  if (auto traversal_neighbor_policy =
          algorithm_config_ptree.get_optional<std::string>(
              "traversal_neighbor_policy")) {
    mapper_config.algorithm_config.traversal_neighbor_policy =
        traversal_neighbor_policy.get();
  }
  if (auto candidate_scope_policy =
          algorithm_config_ptree.get_optional<std::string>(
              "candidate_scope_policy")) {
    mapper_config.algorithm_config.candidate_scope_policy =
        candidate_scope_policy.get();
  }
  if (auto candidate_rank_policy =
          algorithm_config_ptree.get_optional<std::string>(
              "candidate_rank_policy")) {
    mapper_config.algorithm_config.candidate_rank_policy =
        candidate_rank_policy.get();
  }
  if (auto use_yott_annotations =
          algorithm_config_ptree.get_optional<bool>("use_yott_annotations")) {
    mapper_config.algorithm_config.use_yott_annotations =
        use_yott_annotations.get();
  }
  if (auto trace_trials =
          algorithm_config_ptree.get_optional<bool>("trace_trials")) {
    mapper_config.algorithm_config.trace_trials = trace_trials.get();
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
  if (mapper_config.algorithm_config.elite_placement_count.has_value()) {
    algorithm_config_ptree.put(
        "elite_placement_count",
        mapper_config.algorithm_config.elite_placement_count.value());
  }
  if (mapper_config.algorithm_config.io_node_policy.has_value()) {
    algorithm_config_ptree.put(
        "io_node_policy",
        mapper_config.algorithm_config.io_node_policy.value());
  }
  if (mapper_config.algorithm_config.trial_seed_policy.has_value()) {
    algorithm_config_ptree.put(
        "trial_seed_policy",
        mapper_config.algorithm_config.trial_seed_policy.value());
  }
  if (mapper_config.algorithm_config.traversal_order_policy.has_value()) {
    algorithm_config_ptree.put(
        "traversal_order_policy",
        mapper_config.algorithm_config.traversal_order_policy.value());
  }
  if (mapper_config.algorithm_config.traversal_neighbor_policy.has_value()) {
    algorithm_config_ptree.put(
        "traversal_neighbor_policy",
        mapper_config.algorithm_config.traversal_neighbor_policy.value());
  }
  if (mapper_config.algorithm_config.candidate_scope_policy.has_value()) {
    algorithm_config_ptree.put(
        "candidate_scope_policy",
        mapper_config.algorithm_config.candidate_scope_policy.value());
  }
  if (mapper_config.algorithm_config.candidate_rank_policy.has_value()) {
    algorithm_config_ptree.put(
        "candidate_rank_policy",
        mapper_config.algorithm_config.candidate_rank_policy.value());
  }
  if (mapper_config.algorithm_config.use_yott_annotations.has_value()) {
    algorithm_config_ptree.put(
        "use_yott_annotations",
        mapper_config.algorithm_config.use_yott_annotations.value());
  }
  if (mapper_config.algorithm_config.trace_trials.has_value()) {
    algorithm_config_ptree.put(
        "trace_trials", mapper_config.algorithm_config.trace_trials.value());
  }
  ptree.add_child("Algorithm", algorithm_config_ptree);
  ptree.add_child("DFG", dfg_config_ptree);

  boost::property_tree::write_json(file_name, ptree);
}
