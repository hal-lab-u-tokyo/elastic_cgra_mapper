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
  std::string algorithm_type_str =
      GetValueFromPTree<std::string>(algorithm_config_ptree, "type");
  if (algorithm_type_str == "ILPMapper") {
    mapper_config.algorithm_config.algorithm =
        entity::AlgorithmType::kILPMapper;
  } else if (algorithm_type_str == "ILPPlacementMapper") {
    mapper_config.algorithm_config.algorithm =
        entity::AlgorithmType::kPlacementILPMapper;
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
  }
  ptree.add_child("Algorithm", algorithm_config_ptree);
  ptree.add_child("DFG", dfg_config_ptree);

  boost::property_tree::write_json(file_name, ptree);
}
