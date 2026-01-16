#include <remapper/remapping_manager.hpp>

void RemappingManager::SetMappingMatrix(
    const remapper::MappingMatrix& mapping_matrix) {
  tmp_mapping_matrix_ = mapping_matrix;
  prev_transform_op_ =
      remapper::MappingTransformOp(0, 0, remapper::RotateOp::TopIsTop);
};

remapper::MappingTransformOp RemappingManager::GetNextMappingTransformOp() {
  return prev_transform_op_;
}
