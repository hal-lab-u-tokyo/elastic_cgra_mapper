#include <entity/architecture.hpp>
#include <remapper/algorithm_entity.hpp>

remapper::MappingMatrix::MappingMatrix() : id(-1), op_rate(-1) {
  op_num_matrix_ = Eigen::MatrixXi::Zero(0, 0);
  memory_op_num_matrix_ = Eigen::MatrixXi::Zero(0, 0);
  mapping_ = entity::Mapping();

  row_size = 0;
  column_size = 0;
  context_size = 0;
}

remapper::MappingMatrix remapper::MappingMatrix::CreateDummyMappingMatrix(
    entity::MRRGConfig mrrg_config, int _id) {
  Eigen::MatrixXi op_num_matrix =
      Eigen::MatrixXi::Zero(mrrg_config.row, mrrg_config.column);
  Eigen::MatrixXi memory_op_num_matrix =
      Eigen::MatrixXi::Zero(mrrg_config.row, mrrg_config.column);
  for (int row_id = 0; row_id < mrrg_config.row; row_id++) {
    for (int column_id = 0; column_id < mrrg_config.column; column_id++) {
      op_num_matrix(row_id, column_id) = mrrg_config.context_size;
      if (mrrg_config.memory_io == entity::MRRGMemoryIOType::kAll) {
        memory_op_num_matrix(row_id, column_id) = 1;
      } else if (mrrg_config.memory_io == entity::MRRGMemoryIOType::kOneEnd) {
        if (column_id == 0) {
          memory_op_num_matrix(row_id, column_id) = 1;
        }
      } else if (mrrg_config.memory_io == entity::MRRGMemoryIOType::kBothEnds) {
        if (column_id == 0 || column_id == mrrg_config.column - 1) {
          memory_op_num_matrix(row_id, column_id) = 1;
        }
      }
    }
  }
  const auto result = MappingMatrix(op_num_matrix, memory_op_num_matrix, _id,
                                    mrrg_config, mrrg_config);
  return result;
}

remapper::MappingMatrix::MappingMatrix(
    const entity::Mapping& mapping, int _id,
    const entity::MRRGConfig& target_mrrg_config, int parallel_num)
    : id(_id) {
  op_num_matrix_ = Eigen::MatrixXi::Zero(mapping.GetMRRGConfig().row,
                                         mapping.GetMRRGConfig().column);
  memory_op_num_matrix_ = Eigen::MatrixXi::Zero(mapping.GetMRRGConfig().row,
                                                mapping.GetMRRGConfig().column);
  int op_num_without_routing = 0;
  for (int row_id = 0; row_id < mapping.GetMRRGConfig().row; row_id++) {
    for (int column_id = 0; column_id < mapping.GetMRRGConfig().column;
         column_id++) {
      int pe_op_count = 0;
      int pe_memory_op_count = 0;
      for (int context_id = 0;
           context_id < mapping.GetMRRGConfig().context_size; context_id++) {
        const entity::ConfigId config_id(row_id, column_id, context_id);
        const auto config = mapping.GetConfig(config_id);
        if (config.operation_type != entity::OpType::NOP) {
          pe_op_count++;
        }
        if (entity::IsDFGOp(config.operation_type)) {
          op_num_without_routing++;
        }
        if (entity::IsMemoryAccessOperation(config.operation_type)) {
          pe_memory_op_count++;
        }
      }
      op_num_matrix_(row_id, column_id) = pe_op_count;
      memory_op_num_matrix_(row_id, column_id) = pe_memory_op_count;
    }
  }

  row_size = mapping.GetMRRGConfig().row;
  column_size = mapping.GetMRRGConfig().column;

  mapping_ = mapping;
  context_size = op_num_matrix_.maxCoeff();
  parallel_num_ = parallel_num;

  op_rate =
      (double)op_num_without_routing / (row_size * column_size * context_size);
  estimated_reallocate_num =
      (target_mrrg_config.row / mapping.GetMRRGConfig().row) *
      (target_mrrg_config.column / mapping.GetMRRGConfig().column) *
      (target_mrrg_config.context_size / mapping.GetMRRGConfig().context_size);

  // calculate num_waste_of_memory_io
  num_waste_of_memory_io = 0;
  if (mapping.GetMRRGConfig().memory_io == entity::MRRGMemoryIOType::kOneEnd) {
    num_waste_of_memory_io = row_size * context_size;
  } else if (mapping.GetMRRGConfig().memory_io ==
             entity::MRRGMemoryIOType::kBothEnds) {
    num_waste_of_memory_io = 2 * row_size * context_size;
  }
  for (const auto& config : mapping.GetConfigMap()) {
    if (entity::IsMemoryAccessOperation(config.second.operation_type)) {
      num_waste_of_memory_io--;
    }
  }
}

remapper::MappingMatrix::MappingMatrix(
    const Eigen::MatrixXi& op_num_matrix,
    const Eigen::MatrixXi& memory_op_num_matrix, int _id,
    const entity::MRRGConfig mrrg_config,
    const entity::MRRGConfig& target_mrrg_config, int parallel_num)
    : id(_id),
      op_num_matrix_(op_num_matrix),
      memory_op_num_matrix_(memory_op_num_matrix) {
  mapping_ = entity::Mapping(mrrg_config);
  parallel_num_ = parallel_num;
  op_rate = (double)(op_num_matrix.sum()) / mrrg_config.row *
            mrrg_config.column * mrrg_config.context_size;
  estimated_reallocate_num =
      (target_mrrg_config.row / op_num_matrix.rows()) *
      (target_mrrg_config.column / op_num_matrix.cols()) *
      (target_mrrg_config.context_size / op_num_matrix.maxCoeff());
  num_waste_of_memory_io = 0;
  if (mrrg_config.memory_io == entity::MRRGMemoryIOType::kOneEnd) {
    num_waste_of_memory_io =
        mrrg_config.row * mrrg_config.context_size - memory_op_num_matrix.sum();
  } else if (mrrg_config.memory_io == entity::MRRGMemoryIOType::kBothEnds) {
    num_waste_of_memory_io = 2 * mrrg_config.row * mrrg_config.context_size -
                             memory_op_num_matrix.sum();
  }
  row_size = mrrg_config.row;
  column_size = mrrg_config.column;
  context_size = mrrg_config.context_size;
}

Eigen::MatrixXi remapper::MappingMatrix::GetRotatedMatrix(
    const Eigen::MatrixXi& matrix, remapper::RotateOp rotate_op) const {
  if (rotated_matrix_cache_.find(static_cast<int>(rotate_op)) !=
      rotated_matrix_cache_.end()) {
    return rotated_matrix_cache_.at(static_cast<int>(rotate_op));
  }

  Eigen::MatrixXi rotated_matrix;

  if (rotate_op == remapper::RotateOp::TopIsTop) {
    rotated_matrix = matrix;
  } else if (rotate_op == remapper::RotateOp::TopIsLeft) {
    rotated_matrix = matrix.transpose().colwise().reverse().eval();
  } else if (rotate_op == remapper::RotateOp::TopIsBottom) {
    rotated_matrix = matrix.colwise().reverse().rowwise().reverse().eval();
  } else if (rotate_op == remapper::RotateOp::TopIsRight) {
    rotated_matrix = matrix.colwise().reverse().transpose().eval();
  } else if (rotate_op == remapper::RotateOp::TopIsTopMirror) {
    rotated_matrix = matrix.rowwise().reverse().eval();
  } else if (rotate_op == remapper::RotateOp::TopIsLeftMirror) {
    rotated_matrix =
        matrix.transpose().colwise().reverse().rowwise().reverse().eval();
  } else if (rotate_op == remapper::RotateOp::TopIsBottomMirror) {
    rotated_matrix = matrix.colwise().reverse().eval();
  } else if (rotate_op == remapper::RotateOp::TopIsRightMirror) {
    rotated_matrix = matrix.transpose().eval();
  } else {
    assert(false);
  }

  return rotated_matrix;
}

void remapper::MappingMatrix::UpdateRotatedMatrixCache(
    const std::vector<remapper::RotateOp>& rotate_op_vec) {
  for (const auto& rotate_op : rotate_op_vec) {
    if (rotated_matrix_cache_.find(static_cast<int>(rotate_op)) ==
        rotated_matrix_cache_.end()) {
      rotated_matrix_cache_[static_cast<int>(rotate_op)] =
          GetRotatedMatrix(op_num_matrix_, rotate_op);
    }
  }
}

remapper::CGRAMatrix::CGRAMatrix(const entity::MRRGConfig& mrrg_config)
    : mrrg_config_(mrrg_config) {
  row_size = mrrg_config_.row;
  column_size = mrrg_config_.column;
  context_size = mrrg_config_.context_size;

  memory_accessible_matrix_ = Eigen::MatrixXi::Zero(row_size, column_size);
  for (int row_id = 0; row_id < row_size; row_id++) {
    for (int column_id = 0; column_id < column_size; column_id++) {
      if (mrrg_config.memory_io == entity::MRRGMemoryIOType::kAll) {
        memory_accessible_matrix_(row_id, column_id) = 1;
      } else if (mrrg_config.memory_io == entity::MRRGMemoryIOType::kOneEnd) {
        if (column_id == 0) {
          memory_accessible_matrix_(row_id, column_id) = 1;
        }
      } else if (mrrg_config.memory_io == entity::MRRGMemoryIOType::kBothEnds) {
        if (column_id == 0 || column_id == column_size - 1) {
          memory_accessible_matrix_(row_id, column_id) = 1;
        }
      }
    }
  }
}

bool remapper::CGRAMatrix::IsAvailableRemapping(
    const MappingMatrix& mapping_matrix,
    const MappingTransformOp& transform_op) const {
  const auto rotated_matrix =
      mapping_matrix.GetRotatedOpNumMatrix(transform_op.rotate_op);
  if (rotated_matrix.rows() + transform_op.row > row_size ||
      rotated_matrix.cols() + transform_op.column > column_size) {
    return false;
  }

  if (mrrg_config_.memory_io == entity::MRRGMemoryIOType::kAll) {
    return true;
  }

  const auto& rotated_memory_op_num_matrix =
      mapping_matrix.GetRotatedMemoryOpNumMatrix(transform_op.rotate_op);

  const auto& cropped_not_memory_accesible_matrix =
      Eigen::MatrixXi::Ones(rotated_matrix.rows(), rotated_matrix.cols()) -
      memory_accessible_matrix_.block(transform_op.row, transform_op.column,
                                      rotated_matrix.rows(),
                                      rotated_matrix.cols());
  if (cropped_not_memory_accesible_matrix
          .cwiseProduct(rotated_memory_op_num_matrix)
          .sum() > 0) {
    return false;
  }

  return true;
}

int remapper::CGRAMatrix::TryRemapping(
    const std::vector<MappingMatrix>& mapping_matrix_vec,
    const std::vector<MappingTransformOp>& transform_op_vec) const {
  if (mapping_matrix_vec.size() != transform_op_vec.size()) {
    assert("mapping_matrix_vec and transform_op_vec should have the same size");
    abort();
  }

  for (int i = 0; i < mapping_matrix_vec.size(); i++) {
    if (!IsAvailableRemapping(mapping_matrix_vec[i], transform_op_vec[i])) {
      return 0;
    }
  }

  Eigen::MatrixXi op_num_matrix = Eigen::MatrixXi::Zero(row_size, column_size);
  bool over_context_size = false;
  int last_mapping_num = 0;
  for (int i = 0; i < transform_op_vec.size(); i++) {
    last_mapping_num = i;
    const auto& transform_op = transform_op_vec[i];

    Eigen::MatrixXi tmp_matrix =
        mapping_matrix_vec[i].GetRotatedOpNumMatrix(transform_op.rotate_op);

    op_num_matrix.block(transform_op.row, transform_op.column,
                        tmp_matrix.rows(), tmp_matrix.cols()) += tmp_matrix;

    int max_op_num = op_num_matrix.maxCoeff();
    over_context_size = max_op_num > context_size;
    if (over_context_size) break;
  }

  return last_mapping_num + 1;
}
