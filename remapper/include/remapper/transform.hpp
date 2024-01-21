#pragma once

#include <entity/mapping.hpp>

#include "remapper/mapping_transform_op.hpp"

namespace remapper {
entity::Mapping MappingRotater(const entity::Mapping& mapping,
                               const remapper::RotateOp& rotate_op);

entity::MRRGConfig MRRGConfigRotater(const entity::MRRGConfig& mrrg_config,
                                     const remapper::RotateOp& rotate_op);

remapper::RotateOp Rotate180(const remapper::RotateOp& tmp);
}  // namespace remapper