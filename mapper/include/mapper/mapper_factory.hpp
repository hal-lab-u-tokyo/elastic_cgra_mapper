#pragma once

#include <entity/mapper_config.hpp>
#include <mapper/mapper.hpp>

#include <memory>

namespace mapper {

std::unique_ptr<IILPMapper> CreateMapper(
    entity::AlgorithmType algorithm,
    const std::shared_ptr<entity::DFG> dfg_ptr,
    const std::shared_ptr<entity::MRRG> mrrg_ptr);

}  // namespace mapper
