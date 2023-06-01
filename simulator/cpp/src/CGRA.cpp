#include <entity/architecture.hpp>
#include <simulator/CGRA.hpp>

simulator::CGRA::CGRA(entity::MRRGConfig mrrg_config) {
  memory_ptr_ = std::make_shared<simulator::Memory>();
  *memory_ptr_ = simulator::Memory();

  row_ = mrrg_config.row;
  column_ = mrrg_config.column;
  register_size_ = mrrg_config.local_reg_size;
  context_size_ = mrrg_config.context_size;
  num_update_ = 0;

  PE_array_.resize(row_);
  for (int i = 0; i < row_; i++) {
    PE_array_[i].resize(column_);
    for (int j = 0; j < column_; j++) {
      PE_array_[i][j] =
          simulator::PE(register_size_, context_size_, memory_ptr_, i, j);
    }
  }

  for (int row_id = 0; row_id < row_; row_id++) {
    for (int column_id = 0; column_id < column_; column_id++) {
      for (int row_diff = -1; row_diff <= 1; row_diff++) {
        for (int column_diff = -1; column_diff <= 1; column_diff++) {
          int adj_row_id = row_id + row_diff;
          int adj_column_id = column_id + column_diff;

          if (adj_row_id < 0 || row_ <= adj_row_id) continue;
          if (adj_column_id < 0 || column_ <= adj_column_id) continue;
          if (row_diff == 0 && column_diff == 0) continue;

          bool is_diagonal = (abs(row_diff) + abs(column_diff)) == 2;
          if (mrrg_config.network_type ==
                  entity::MRRGNetworkType::kOrthogonal &&
              is_diagonal)
            continue;

          entity::PEPositionId adj_position_id(adj_row_id, adj_column_id);
          if (mrrg_config.cgra_type == entity::MRRGCGRAType::kDefault) {
            simulator::Wire<int> new_output_wire;
            PE_array_[row_id][column_id].SetOutputWire(adj_position_id,
                                                       new_output_wire);
            entity::PEPositionId position_id(row_id, column_id);
            simulator::Wire<int> adj_output_wire =
                PE_array_[adj_row_id][adj_column_id].GetOutputWire(position_id);
            PE_array_[row_id][column_id].SetInputWire(adj_position_id,
                                                      adj_output_wire);
          } else if (mrrg_config.cgra_type == entity::MRRGCGRAType::kElastic) {
          }
        }
      }
    }
  }
}

void simulator::CGRA::SetConfig(std::shared_ptr<entity::Mapping> mapping) {
  entity::ConfigMap config_map = mapping->GetConfigMap();
  for (auto itr = config_map.begin(); itr != config_map.end(); itr++) {
    entity::ConfigId tmp_config_id = itr->first;
    PE_array_[tmp_config_id.row_id][tmp_config_id.column_id].SetConfig(
        tmp_config_id.context_id, config_map[tmp_config_id]);
    entity::CGRAConfig tmp_config = itr->second;
    if (tmp_config.operation_type == entity::OpType::OUTPUT) {
      output_config_id_ = tmp_config_id;
    }
  }
  num_update_ = 0;
}

void simulator::CGRA::Update() {
  for (int row_id = 0; row_id < row_; row_id++) {
    for (int column_id = 0; column_id < column_; column_id++) {
      PE_array_[row_id][column_id].Update();
    }
  }
  if (num_update_ % context_size_ == output_config_id_.context_id) {
    output_vec_.push_back(
        PE_array_[output_config_id_.row_id][output_config_id_.column_id]
            .GetOutput());
  }
  num_update_++;
}

void simulator::CGRA::RegisterUpdate() {
  for (int row_id = 0; row_id < row_; row_id++) {
    for (int column_id = 0; column_id < column_; column_id++) {
      PE_array_[row_id][column_id].RegisterUpdate();
    }
  }
}

void simulator::CGRA::StoreMemoryData(int address, int store_data) {
  memory_ptr_->Store(address, store_data);
}