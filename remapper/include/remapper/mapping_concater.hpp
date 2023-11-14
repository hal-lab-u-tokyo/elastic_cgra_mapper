#pragma once
#include <entity/mapping.hpp>

#include "remapper/mapping_transform_op.hpp"

namespace remapper {
entity::Mapping MappingConcater(
    const std::vector<entity::Mapping>& mapping_vec,
    const std::vector<MappingTransformOp>& mapping_transform_op_vec,
    const entity::MRRGConfig& target_mrrg_config);
}