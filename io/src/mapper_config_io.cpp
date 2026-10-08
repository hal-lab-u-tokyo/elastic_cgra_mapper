#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <io/json_reader.hpp>
#include <io/mapper_config_io.hpp>
#include <mapper/yott_config.hpp>

entity::YOTTConfig io::YOTTConfigFactory(
    boost::property_tree::ptree algorithm_config_ptree) {
  entity::YOTTConfig yott_config;
  yott_config.algorithm = entity::AlgorithmType::kYOTTMapper;

  yott_config.type =
      GetValueFromPTree<std::string>(algorithm_config_ptree, "type");
  yott_config.accept_feasible_solution = GetValueFromPTree<bool>(
      algorithm_config_ptree, "accept_feasible_solution");
  yott_config.placement_only =
      GetValueFromPTree<bool>(algorithm_config_ptree, "placement_only");
  yott_config.max_trials =
      GetValueFromPTree<int>(algorithm_config_ptree, "max_trials");
  yott_config.seed_count =
      GetValueFromPTree<int>(algorithm_config_ptree, "seed_count");
  yott_config.routing_retry_count =
      GetValueFromPTree<int>(algorithm_config_ptree, "routing_retry_count");
  yott_config.random_seed =
      GetValueFromPTree<int>(algorithm_config_ptree, "random_seed");
  yott_config.max_iterations =
      GetValueFromPTree<int>(algorithm_config_ptree, "max_iterations");

  return yott_config;
}

boost::property_tree::ptree io::YOTTConfigToPTree(
    const mapper::YOTTConfig& yott_config) {
  boost::property_tree::ptree algorithm_config_ptree;
  algorithm_config_ptree.put("type", "YOTTMapper");
  algorithm_config_ptree.put("accept_feasible_solution",
                             yott_config.accept_feasible_solution);
  algorithm_config_ptree.put("placement_only", yott_config.placement_only);
  algorithm_config_ptree.put("max_trials", yott_config.max_trials);
  algorithm_config_ptree.put("seed_count", yott_config.seed_count);
  algorithm_config_ptree.put("routing_retry_count",
                             yott_config.routing_retry_count);
  algorithm_config_ptree.put("random_seed", yott_config.random_seed);
  algorithm_config_ptree.put("max_iterations", yott_config.max_iterations);

  return algorithm_config_ptree;
}

entity::MapperConfig io::ReadMapperConfigFromJsonFile(std::string file_name) {
  boost::property_tree::ptree ptree;
  boost::property_tree::read_json(file_name, ptree);

  entity::MapperConfig mapper_config;

  boost::property_tree::ptree dfg_config_ptree = ptree.get_child("DFG");
  mapper_config.dfg_config.operation_name_label =
      GetValueFromPTree<std::string>(dfg_config_ptree, "operation_name_label");

  boost::property_tree::ptree algorithm_config_ptree =
      ptree.get_child("Algorithm");
  std::string algorithm_type_str =
      GetValueFromPTree<std::string>(algorithm_config_ptree, "type");
  if (algorithm_type_str == "ILPMapper") {
    mapper_config.algorithm_config.algorithm =
        entity::AlgorithmType::kILPMapper;
  } else if (algorithm_type_str == "ILPPlacementMapper") {
    mapper_config.algorithm_config.algorithm =
        entity::AlgorithmType::kPlacementILPMapper;
  } else if (algorithm_type_str == "YOTTMapper") {
    mapper_config.algorithm_config = YOTTConfigFactory(algorithm_config_ptree);
  } else {
    std::cerr << "Invalid algorithm type in mapper config: "
              << algorithm_type_str << std::endl;
    abort();
  }
  return mapper_config;
}

void io::WriteMapperConfigToJsonFile(
    std::string file_name, const entity::MapperConfig& mapper_config) {
  boost::property_tree::ptree dfg_config_ptree, ptree, algorithm_config_ptree;

  dfg_config_ptree.put("operation_name_label",
                       mapper_config.dfg_config.operation_name_label);

  switch (mapper_config.algorithm_config.algorithm) {
    case entity::AlgorithmType::kILPMapper:
      algorithm_config_ptree.put("type", "ILPMapper");
      break;
    case entity::AlgorithmType::kPlacementILPMapper:
      algorithm_config_ptree.put("type", "ILPPlacementMapper");
      break;
    case entity::AlgorithmType::kYOTTMapper:
      algorithm_config_ptree =
          YOTTConfigToPTree(static_cast<const mapper::YOTTConfig&>(
              mapper_config.algorithm_config));
      break;
  }

  ptree.add_child("Algorithm", algorithm_config_ptree);
  ptree.add_child("DFG", dfg_config_ptree);

  boost::property_tree::write_json(file_name, ptree);
}
