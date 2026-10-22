#pragma once

#include <mapper/mapper.hpp>

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace mapper {

using MapperCreator = std::function<std::unique_ptr<IMapper>(
    const std::shared_ptr<entity::DFG>& dfg_ptr,
    const std::shared_ptr<entity::MRRG>& mrrg_ptr)>;

bool RegisterMapper(const std::string& type, MapperCreator creator);

template <typename MapperT>
bool RegisterMapperType(const std::string& type) {
  return RegisterMapper(
      type, [](const std::shared_ptr<entity::DFG>& dfg_ptr,
               const std::shared_ptr<entity::MRRG>& mrrg_ptr) {
        return std::make_unique<MapperT>(dfg_ptr, mrrg_ptr);
      });
}

std::unique_ptr<IMapper> CreateMapper(
    const std::string& algorithm_type,
    const std::shared_ptr<entity::DFG> dfg_ptr,
    const std::shared_ptr<entity::MRRG> mrrg_ptr);

std::vector<std::string> GetRegisteredMapperTypes();

}  // namespace mapper
