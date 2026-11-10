#include <cstdlib>
#include <iostream>
#include <map>
#include <mapper/mapper_factory.hpp>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace {

struct Registration {
  mapper::MapperCreator mapper;
  mapper::ConfigCreator config;
};

std::map<std::string, Registration>& MapperRegistry() {
  static std::map<std::string, Registration> registry;
  return registry;
}

std::string JoinRegisteredMapperTypes() {
  std::ostringstream oss;
  bool first = true;
  for (const auto& entry : MapperRegistry()) {
    if (!first) {
      oss << ", ";
    }
    oss << entry.first;
    first = false;
  }
  return oss.str();
}

}  // namespace

bool mapper::RegisterMapper(const std::string& type, MapperCreator creator,
                            ConfigCreator config_creator) {
  auto& registry = MapperRegistry();
  if (registry.count(type) > 0) {
    std::cerr << "Duplicate mapper type registration: " << type << std::endl;
    std::abort();
  }
  registry.emplace(type,
                   Registration{std::move(creator), std::move(config_creator)});
  return true;
}

std::unique_ptr<mapper::IMapper> mapper::CreateMapper(
    const std::string& algorithm_type,
    const std::shared_ptr<entity::DFG> dfg_ptr,
    const std::shared_ptr<entity::MRRG> mrrg_ptr) {
  const auto& registry = MapperRegistry();
  auto mapper_it = registry.find(algorithm_type);
  if (mapper_it != registry.end()) {
    return mapper_it->second.mapper(dfg_ptr, mrrg_ptr);
  }

  std::cerr << "Invalid mapper type in mapper config: " << algorithm_type
            << std::endl;
  std::cerr << "Registered mapper types: " << JoinRegisteredMapperTypes()
            << std::endl;
  std::abort();
}

std::vector<std::string> mapper::GetRegisteredMapperTypes() {
  std::vector<std::string> types;
  for (const auto& entry : MapperRegistry()) {
    types.push_back(entry.first);
  }
  return types;
}

std::unique_ptr<mapper::AlgorithmConfig> mapper::CreateAlgorithmConfig(
    const std::string& type) {
  const auto& registry = MapperRegistry();
  auto entry = registry.find(type);
  if (entry == registry.end() || !entry->second.config) {
    throw std::invalid_argument("No algorithm configuration registered for: " +
                                type);
  }
  auto result = entry->second.config();
  result->type =
      type;  // Preserve the exact legacy variant/alias selected by the file.
  return result;
}
