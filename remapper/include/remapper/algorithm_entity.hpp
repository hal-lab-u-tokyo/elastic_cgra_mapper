#pragma once
#include <Eigen/Dense>
#include <entity/mapping.hpp>
#include <entity/mrrg.hpp>
#include <remapper/mapping_transform_op.hpp>

namespace remapper {

class Rectangle {
 public:
  int row_size;
  int column_size;
  int context_size;
};

class MappingMatrix : public Rectangle {
 public:
  MappingMatrix();
  MappingMatrix(const entity::Mapping& mapping, int _id);

  double op_rate;
  double num_waste_of_memory_io;
  int id;

  entity::ConfigMap GetConfigMap() const { return mapping_.GetConfigMap(); };
  Eigen::MatrixXi GetOpNumMatrix() const { return op_num_matrix_; };
  Eigen::MatrixXi GetMemoryOpNumMatrix() const {
    return memory_op_num_matrix_;
  };
  Eigen::MatrixXi GetRotatedOpNumMatrix(remapper::RotateOp rotate_op) const {
    return GetRotatedMatrix(op_num_matrix_, rotate_op);
  };
  Eigen::MatrixXi GetRotatedMemoryOpNumMatrix(
      remapper::RotateOp rotate_op) const {
    return GetRotatedMatrix(memory_op_num_matrix_, rotate_op);
  };
  entity::Mapping GetMapping() const { return mapping_; };

 private:
  Eigen::MatrixXi op_num_matrix_;
  Eigen::MatrixXi memory_op_num_matrix_;
  entity::Mapping mapping_;
  Eigen::MatrixXi GetRotatedMatrix(const Eigen::MatrixXi& matrix,
                                   remapper::RotateOp rotate_op) const;
};

class CGRAMatrix : public Rectangle {
 public:
  CGRAMatrix(const entity::MRRGConfig& mrrg_config);
  bool IsAvailableRemapping(const MappingMatrix& mapping_matrix,
                            const MappingTransformOp& transform_op) const;
  entity::MRRGConfig GetMRRGConfig() const { return mrrg_config_; };

 private:
  entity::MRRGConfig mrrg_config_;
};
}  // namespace remapper