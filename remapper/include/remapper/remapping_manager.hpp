#pragma once
#include <entity/mapping.hpp>
#include <remapper/algorithm_entity.hpp>
#include <remapper/mapping_transform_op.hpp>

class RemappingManager {
 public:
  RemappingManager(
      const remapper::CGRAMatrix& cgra_matrix,
      const remapper::RotateType& rotate_type = remapper::RotateType::All)
      : cgra_matrix_(cgra_matrix), rotate_type_(rotate_type){};
  ~RemappingManager() = default;
  void SetMappingMatrix(const remapper::MappingMatrix& mapping_matrix);
  remapper::MappingTransformOp GetNextMappingTransformOp();
  bool SearchAllTransform() const;

 private:
  remapper::CGRAMatrix cgra_matrix_;
  remapper::RotateType rotate_type_;
  remapper::MappingMatrix tmp_mapping_matrix_;
  remapper::MappingTransformOp prev_transform_op_;
};
