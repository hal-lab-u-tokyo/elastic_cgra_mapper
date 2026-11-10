#include <boost/property_tree/ptree.hpp>
#include <mapper/configuration.hpp>
#include <mapper/ilp/configuration.hpp>
#include <mapper/mapper.hpp>
#include <mapper/mapper_factory.hpp>
#include <mapper/placement_first/configuration.hpp>
#include <mapper/prisa/configuration.hpp>
#include <mapper/sa/configuration.hpp>
#include <mapper/yoto/configuration.hpp>
#include <mapper/yott/configuration.hpp>
#include <mapper/yott_core/configuration.hpp>
#include <mapper/yott_core_repair/configuration.hpp>
#include <stdexcept>

namespace mapper {

void IMapper::Configure(const MapperOptions& options) {
  SetLogFilePath(options.log_file_path);
  SetTimeOut(options.timeout_s);
  SetAcceptFeasibleSolution(options.accept_feasible_solution);
  SetPlacementOnly(options.placement_only);
}

void AlgorithmConfig::Apply(IMapper& target) const {
  Validate();
  ApplyOptions(target);
}

void AlgorithmConfig::CheckKnownOptions(
    const boost::property_tree::ptree& tree,
    std::initializer_list<const char*> names) {
  const std::set<std::string> known(names.begin(), names.end());
  for (const auto& entry : tree) {
    if (!known.count(entry.first))
      throw std::invalid_argument("Unknown or unused algorithm option: " +
                                  entry.first);
  }
}

void AlgorithmConfig::Validate() const {
  const auto registered = CreateAlgorithmConfig(type);
  if (!registered->supported_options)
    return;  // Custom registration compatibility.
  boost::property_tree::ptree options;
  WriteOptions(options);
  for (const auto& entry : options) {
    const auto value = entry.second.get_value<std::string>();
    if (value == "auto" ||
        (entry.first == "placement_only" && value == "false"))
      continue;
    if (!registered->supported_options->count(entry.first))
      throw std::invalid_argument("Option " + entry.first +
                                  " is not consumed by mapper " + type);
  }
}

MapperConfig::MapperConfig() : algorithm_config(std::make_unique<ILPConfig>()) {
  algorithm_config->type = "PlacementILPMapper";
  algorithm_defaults.emplace("ILP", std::make_unique<ILPConfig>());
  algorithm_defaults.emplace("SA", std::make_unique<SAConfig>());
  algorithm_defaults.emplace("PRISA", std::make_unique<PRISAConfig>());
  algorithm_defaults.emplace("YOTO", std::make_unique<YOTOConfig>());
  algorithm_defaults.emplace("YOTT", std::make_unique<YOTTConfig>());
  algorithm_defaults.emplace("YOTTCore", std::make_unique<YOTTCoreConfig>());
  algorithm_defaults.emplace("YOTTCoreRepair",
                             std::make_unique<YOTTCoreRepairConfig>());
  algorithm_defaults.emplace("PlacementFirst",
                             std::make_unique<PlacementFirstConfig>());
}

}  // namespace mapper
