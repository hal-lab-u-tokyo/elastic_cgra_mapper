#pragma once

#include <boost/property_tree/ptree_fwd.hpp>
#include <entity/dfg_config.hpp>
#include <initializer_list>
#include <map>
#include <mapper/mapper.hpp>
#include <memory>
#include <optional>
#include <set>
#include <string>

namespace mapper {

// Persistent execution policy common to mappers; CLI log/timeout stay in
// runtime arguments. Family options live in derived types.
struct AlgorithmConfig {
  virtual ~AlgorithmConfig() = default;
  std::string type;
  void Apply(IMapper& target) const;
  void Validate() const;
  // Registry metadata, not serialized options or a second alias table.
  std::optional<std::set<std::string>> supported_options;
  virtual const char* FamilyName() const = 0;
  virtual void ReadOptions(const boost::property_tree::ptree& tree) = 0;
  virtual void WriteOptions(boost::property_tree::ptree& tree) const = 0;

 protected:
  static void CheckKnownOptions(const boost::property_tree::ptree& tree,
                                std::initializer_list<const char*> names);
  virtual void ApplyOptions(IMapper& target) const = 0;
};

struct MapperConfig {
  MapperConfig();
  entity::DFGConfig dfg_config;
  std::unique_ptr<AlgorithmConfig> algorithm_config;
  // Typed defaults for all families, including those not selected by Exec.type.
  std::map<std::string, std::unique_ptr<AlgorithmConfig>> algorithm_defaults;
};
}  // namespace mapper
