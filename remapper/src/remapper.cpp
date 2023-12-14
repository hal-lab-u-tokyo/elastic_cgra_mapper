#include "remapper/remapper.hpp"

#include <Eigen/Eigen>

#include "remapper/combination_counter.hpp"
#include "remapper/dp_elastic_remapper.hpp"
#include "remapper/full_search_elastic_remapper.hpp"
#include "remapper/greedy_elastic_remapper.hpp"
#include "remapper/mapping_concater.hpp"
#include "remapper/mapping_transform_op.hpp"
#include "remapper/rotater.hpp"
#include "time.h"

Eigen::MatrixXi remapper::CreateMatrixForElastic(
    const entity::Mapping& mapping) {
  int min_row_id = mapping.GetMRRGConfig().row,
      min_column_id = mapping.GetMRRGConfig().column;
  int max_row_id = 0, max_column_id = 0;
  Eigen::MatrixXi matrix = Eigen::MatrixXi::Zero(
      mapping.GetMRRGConfig().row, mapping.GetMRRGConfig().column);
  for (int row_id = 0; row_id < mapping.GetMRRGConfig().row; row_id++) {
    for (int column_id = 0; column_id < mapping.GetMRRGConfig().column;
         column_id++) {
      int op_count = 0;
      for (int context_id = 0;
           context_id < mapping.GetMRRGConfig().context_size; context_id++) {
        const entity::ConfigId config_id(row_id, column_id, context_id);
        const auto config = mapping.GetConfig(config_id);
        if (config.operation_type != entity::OpType::NOP) {
          op_count++;
        }
      }
      if (op_count > 0) {
        min_row_id = std::min(row_id, min_row_id);
        max_row_id = std::max(row_id, max_row_id);
        min_column_id = std::min(column_id, min_column_id);
        max_column_id = std::max(column_id, max_column_id);
      }
      matrix(row_id, column_id) = op_count;
    }
  }

  const int row_size = max_row_id - min_row_id + 1;
  const int column_size = max_column_id - min_column_id + 1;
  return matrix.block(min_row_id, min_column_id, row_size, column_size);
}

Eigen::MatrixXi remapper::CreateMatrixForElastic(
    const entity::Mapping& mapping,
    const entity::MRRGConfig& target_mrrg_config,
    const remapper::MappingTransformOp transform_op) {
  Eigen::MatrixXi matrix =
      Eigen::MatrixXi::Zero(target_mrrg_config.row, target_mrrg_config.column);
  const entity::Mapping rotated_mapping =
      remapper::MappingRotater(mapping, transform_op.rotate_op);

  for (int i = 0; i < rotated_mapping.GetMRRGConfig().row; i++) {
    for (int j = 0; j < rotated_mapping.GetMRRGConfig().column; j++) {
      int op_count = 0;
      for (int k = 0; k < rotated_mapping.GetMRRGConfig().context_size; k++) {
        const entity::ConfigId config_id(i, j, k);
        const auto config = mapping.GetConfig(config_id);
        if (config.operation_type != entity::OpType::NOP) {
          op_count++;
        } else {
          break;
        }
      }
      matrix(i + transform_op.row, j + transform_op.column) = op_count;
    }
  }

  return matrix;
}

std::pair<bool, entity::Mapping> remapper::Remapper::ElasticRemapping(
    const std::vector<entity::Mapping>& mapping_vec,
    const entity::MRRGConfig& target_mrrg_config, const int target_parallel_num,
    std::ofstream& log_file, remapper::RemappingMode mode) {
  switch (mode) {
    case RemappingMode::FullSearch:
      return remapper::FullSearchElasticRemapping(mapping_vec, target_mrrg_config,
                                        target_parallel_num, log_file);
    case RemappingMode::Greedy:
      return remapper::GreedyElasticRemapping(mapping_vec, target_mrrg_config,
                                    target_parallel_num, log_file);
    case RemappingMode::DP:
      return remapper::DPElasticRemapping(mapping_vec, target_mrrg_config,
                                target_parallel_num, log_file);
  }
}

remapper::MappingRectangle::MappingRectangle(int _id,
                                             const Eigen::MatrixXi& _matrix,
                                             const entity::Mapping& mapping) {
  row = _matrix.rows();
  column = _matrix.cols();
  config = _matrix.maxCoeff();
  int op_num_without_routing = 0;
  for (const auto& config : mapping.GetConfigMap()) {
    if (config.second.operation_type != entity::OpType::NOP &&
        config.second.operation_type != entity::OpType::ROUTE) {
      op_num_without_routing++;
    }
  }
  op_rate = (double)op_num_without_routing / (row * column * config);
  id = _id;

  if (mapping.GetMRRGConfig().memory_io == entity::MRRGMemoryIOType::kAll) {
    num_waste_of_memory_io = 0;
  } else {
    if (mapping.GetMRRGConfig().memory_io ==
        entity::MRRGMemoryIOType::kOneEnd) {
      num_waste_of_memory_io = row * config;
    } else if (mapping.GetMRRGConfig().memory_io ==
               entity::MRRGMemoryIOType::kBothEnds) {
      num_waste_of_memory_io = 2 * row * column;
    }

    for (const auto& config : mapping.GetConfigMap()) {
      if (config.second.operation_type == entity::OpType::LOAD ||
          config.second.operation_type == entity::OpType::STORE ||
          config.second.operation_type == entity::OpType::OUTPUT) {
        num_waste_of_memory_io--;
      }
    }
  }
}

bool remapper::IsAvailableRemapping(const entity::Mapping& mapping, int row_shift,
                          int column_shift,
                          const entity::MRRGConfig& target_mrrg_config) {
  if (target_mrrg_config.memory_io == entity::MRRGMemoryIOType::kAll) {
    return true;
  }

  const auto& config_map = mapping.GetConfigMap();
  for (const auto& config_id_and_config : config_map) {
    const auto& op = config_id_and_config.second.operation_type;

    if (op != entity::OpType::LOAD && op != entity::OpType::OUTPUT &&
        op != entity::OpType::STORE) {
      continue;
    }

    int column_id = config_id_and_config.first.column_id + column_shift;
    if (column_id != 0 && column_id != target_mrrg_config.column - 1) {
      return false;
    }
  }

  return true;
}
