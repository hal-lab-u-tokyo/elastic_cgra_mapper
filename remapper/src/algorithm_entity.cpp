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
      op_num_matrix(row_id, column_id) = 1;
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
  const auto result = MappingMatrix(
      op_num_matrix, memory_op_num_matrix, _id, mrrg_config.row,
      mrrg_config.column, mrrg_config.context_size, mrrg_config.memory_io);
  return result;
}

remapper::MappingMatrix::MappingMatrix(const entity::Mapping& mapping, int _id)
    : id(_id) {
  int min_row_id = mapping.GetMRRGConfig().row,
      min_column_id = mapping.GetMRRGConfig().column;
  int max_row_id = 0, max_column_id = 0;
  Eigen::MatrixXi op_num_matrix = Eigen::MatrixXi::Zero(
      mapping.GetMRRGConfig().row, mapping.GetMRRGConfig().column);
  Eigen::MatrixXi memory_op_num_matrix = Eigen::MatrixXi::Zero(
      mapping.GetMRRGConfig().row, mapping.GetMRRGConfig().column);
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
        if (!entity::IsDFGOp(config.operation_type)) {
          op_num_without_routing++;
        }
        if (entity::IsMemoryAccessOperation(config.operation_type)) {
          pe_memory_op_count++;
        }
      }
      if (pe_op_count > 0) {
        min_row_id = std::min(row_id, min_row_id);
        max_row_id = std::max(row_id, max_row_id);
        min_column_id = std::min(column_id, min_column_id);
        max_column_id = std::max(column_id, max_column_id);
      }
      op_num_matrix(row_id, column_id) = pe_op_count;
      memory_op_num_matrix(row_id, column_id) = pe_memory_op_count;
    }
  }

  row_size = max_row_id - min_row_id + 1;
  column_size = max_column_id - min_column_id + 1;

  op_num_matrix_ =
      op_num_matrix.block(min_row_id, min_column_id, row_size, column_size);
  memory_op_num_matrix_ = memory_op_num_matrix.block(min_row_id, min_column_id,
                                                     row_size, column_size);
  mapping_ = mapping;
  context_size = op_num_matrix_.maxCoeff();

  op_rate =
      (double)op_num_without_routing / (row_size * column_size * context_size);

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
    const Eigen::MatrixXi& memory_op_num_matrix, int _id, int _row_size,
    int _column_size, int _context_size, entity::MRRGMemoryIOType memory_io)
    : id(_id),
      op_num_matrix_(op_num_matrix),
      memory_op_num_matrix_(memory_op_num_matrix) {
  mapping_ = entity::Mapping();
  op_rate =
      (double)(op_num_matrix.sum()) / _row_size * _column_size * _context_size;
  num_waste_of_memory_io = 0;
  if (memory_io == entity::MRRGMemoryIOType::kOneEnd) {
    num_waste_of_memory_io =
        _row_size * _context_size - memory_op_num_matrix.sum();
  } else if (memory_io == entity::MRRGMemoryIOType::kBothEnds) {
    num_waste_of_memory_io =
        2 * _row_size * _context_size - memory_op_num_matrix.sum();
  }
  row_size = _row_size;
  column_size = _column_size;
  context_size = _context_size;
}

Eigen::MatrixXi remapper::MappingMatrix::GetRotatedMatrix(
    const Eigen::MatrixXi& matrix, remapper::RotateOp rotate_op) const {
  if (rotate_op == remapper::RotateOp::TopIsTop) {
    return matrix;
  } else if (rotate_op == remapper::RotateOp::TopIsLeft) {
    return matrix.transpose().colwise().reverse().eval();
  } else if (rotate_op == remapper::RotateOp::TopIsBottom) {
    return matrix.colwise().reverse().rowwise().reverse().eval();
  } else if (rotate_op == remapper::RotateOp::TopIsRight) {
    return matrix.colwise().reverse().transpose().eval();
  } else {
    assert(false);
  }
}

remapper::CGRAMatrix::CGRAMatrix(const entity::MRRGConfig& mrrg_config)
    : mrrg_config_(mrrg_config) {
  row_size = mrrg_config_.row;
  column_size = mrrg_config_.column;
  context_size = mrrg_config_.context_size;
}

bool remapper::CGRAMatrix::IsAvailableRemapping(
    const MappingMatrix& mapping_matrix,
    const MappingTransformOp& transform_op) const {
  if (mrrg_config_.memory_io == entity::MRRGMemoryIOType::kAll) {
    return true;
  }

  const auto& rotated_memory_op_num_matrix =
      mapping_matrix.GetRotatedMemoryOpNumMatrix(transform_op.rotate_op);

  for (int row_id = 0; row_id < rotated_memory_op_num_matrix.rows(); row_id++) {
    for (int column_id = 1; column_id < rotated_memory_op_num_matrix.cols() - 1;
         column_id++) {
      if (rotated_memory_op_num_matrix(row_id, column_id) > 0) {
        return false;
      }
    }
  }

  return true;
}