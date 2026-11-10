#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <fstream>
#include <mapper/io/configuration_io.hpp>
#include <mapper/mapper_factory.hpp>
#include <sstream>
#include <stdexcept>
#include <typeinfo>

namespace mapper {
namespace {
void ValidateType(const AlgorithmConfig& config) {
  const auto registered = CreateAlgorithmConfig(config.type);
  if (typeid(*registered) != typeid(config)) {
    throw std::invalid_argument(
        "Algorithm type does not match its configuration object: " +
        config.type);
  }
}
}  // namespace

MapperConfig ReadMapperConfigFromJsonFile(const std::string& file_name) {
  boost::property_tree::ptree tree;
  boost::property_tree::read_json(file_name, tree);
  MapperConfig result;
  result.dfg_config.operation_name_label =
      tree.get<std::string>("DFG.operation_name_label");
  // Old experiment inputs used a flat Algorithm section. New files separate
  // execution settings from the per-family defaults bank.
  const bool legacy = !tree.get_child_optional("Exec");
  const auto& exec = tree.get_child(legacy ? "Algorithm" : "Exec");
  result.algorithm_config =
      CreateAlgorithmConfig(exec.get<std::string>("type"));
  auto& config = *result.algorithm_config;
  if (legacy) {
    boost::property_tree::ptree supported;
    config.WriteOptions(supported);
    boost::property_tree::ptree filtered;
    for (const auto& entry : exec) {
      if (supported.count(entry.first) &&
          (!config.supported_options ||
           config.supported_options->count(entry.first)))
        filtered.add_child(entry.first, entry.second);
    }
    // Legacy files contained fields that existing variants silently ignored.
    // Do not turn those old no-ops into new behavior or reject saved
    // experiments.
    config.ReadOptions(filtered);
    boost::property_tree::ptree options;
    config.WriteOptions(options);
    result.algorithm_defaults.at(config.FamilyName())->ReadOptions(options);
  } else {
    for (const auto& entry : exec) {
      if (entry.first != "type")
        throw std::invalid_argument(
            "Exec contains only type; move algorithm options into Algorithm: " +
            entry.first);
    }
    const auto& bank = tree.get_child("Algorithm");
    for (const auto& entry : bank) {
      const auto found = result.algorithm_defaults.find(entry.first);
      if (found == result.algorithm_defaults.end()) {
        throw std::invalid_argument("Unknown algorithm defaults family: " +
                                    entry.first);
      }
      found->second->ReadOptions(entry.second);
    }
    boost::property_tree::ptree options;
    result.algorithm_defaults.at(config.FamilyName())->WriteOptions(options);
    config.ReadOptions(options);
  }
  config.Validate();
  return result;
}

void WriteMapperConfigToJsonFile(const std::string& file_name,
                                 const MapperConfig& input) {
  if (!input.algorithm_config)
    throw std::invalid_argument("Missing algorithm configuration");
  const auto& config = *input.algorithm_config;
  ValidateType(config);
  config.Validate();
  boost::property_tree::ptree tree, exec, bank;
  tree.put("DFG.operation_name_label", input.dfg_config.operation_name_label);
  exec.put("type", config.type);
  for (const auto& entry : input.algorithm_defaults) {
    if (!entry.second || entry.first != entry.second->FamilyName()) {
      throw std::invalid_argument("Invalid algorithm defaults family: " +
                                  entry.first);
    }
    boost::property_tree::ptree options;
    entry.second->WriteOptions(options);
    bank.add_child(entry.first, options);
  }
  // The selected configuration is authoritative after callers edit its options.
  boost::property_tree::ptree selected;
  config.WriteOptions(selected);
  bank.put_child(config.FamilyName(), selected);
  tree.add_child("Exec", exec);
  tree.add_child("Algorithm", bank);
  // property_tree emits an empty node as a string. Family entries are objects,
  // including the two families with no algorithm-specific options.
  std::ostringstream stream;
  boost::property_tree::write_json(stream, tree);
  auto json = stream.str();
  for (const auto& entry : bank) {
    if (entry.second.empty()) {
      const auto empty = "\"" + entry.first + "\": \"\"";
      const auto position = json.find(empty);
      if (position != std::string::npos)
        json.replace(position, empty.size(), "\"" + entry.first + "\": {}");
    }
  }
  std::ofstream output(file_name);
  if (!output)
    throw std::runtime_error("Cannot write mapper configuration: " + file_name);
  output << json;
  if (!output)
    throw std::runtime_error("Failed writing mapper configuration: " +
                             file_name);
}
}  // namespace mapper
