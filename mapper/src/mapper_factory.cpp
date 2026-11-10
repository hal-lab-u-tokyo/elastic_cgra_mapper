#include <mapper/mapper_factory.hpp>

#include <cstdlib>
#include <iostream>
#include <map>
#include <sstream>
#include <utility>

namespace {

std::map<std::string, mapper::MapperCreator>& MapperRegistry() {
  static std::map<std::string, mapper::MapperCreator> registry;
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

bool mapper::RegisterMapper(const std::string& type, MapperCreator creator) {
  auto& registry = MapperRegistry();
  if (registry.count(type) > 0) {
    std::cerr << "Duplicate mapper type registration: " << type << std::endl;
    std::abort();
  }
  registry.emplace(type, std::move(creator));
  return true;
}

std::unique_ptr<mapper::IMapper> mapper::CreateMapper(
    const std::string& algorithm_type,
    const std::shared_ptr<entity::DFG> dfg_ptr,
    const std::shared_ptr<entity::MRRG> mrrg_ptr) {
  const auto& registry = MapperRegistry();
  auto mapper_it = registry.find(algorithm_type);
  if (mapper_it != registry.end()) {
    return mapper_it->second(dfg_ptr, mrrg_ptr);
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
