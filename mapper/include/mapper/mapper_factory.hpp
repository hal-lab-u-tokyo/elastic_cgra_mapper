#pragma once

#include <functional>
#include <mapper/configuration.hpp>
#include <mapper/mapper.hpp>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace mapper {

using MapperCreator = std::function<std::unique_ptr<IMapper>(
    const std::shared_ptr<entity::DFG>& dfg_ptr,
    const std::shared_ptr<entity::MRRG>& mrrg_ptr)>;

using ConfigCreator = std::function<std::unique_ptr<AlgorithmConfig>()>;

bool RegisterMapper(const std::string& type, MapperCreator creator,
                    ConfigCreator config_creator = {});
std::unique_ptr<AlgorithmConfig> CreateAlgorithmConfig(const std::string& type);

template <typename MapperT, typename ConfigT = void>
bool RegisterMapperType(const std::string& type) {
  MapperCreator creator = [](const std::shared_ptr<entity::DFG>& dfg,
                             const std::shared_ptr<entity::MRRG>& mrrg) {
    return std::make_unique<MapperT>(dfg, mrrg);
  };
  if constexpr (std::is_void_v<ConfigT>) {
    return RegisterMapper(type, std::move(creator));
  } else {
    return RegisterMapper(type, std::move(creator),
                          [] { return std::make_unique<ConfigT>(); });
  }
}

std::unique_ptr<IMapper> CreateMapper(
    const std::string& algorithm_type,
    const std::shared_ptr<entity::DFG> dfg_ptr,
    const std::shared_ptr<entity::MRRG> mrrg_ptr);

std::vector<std::string> GetRegisteredMapperTypes();

}  // namespace mapper
