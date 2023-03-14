#include <simulator/CGRA.hpp>
#include <entity/architecture.hpp>

entity::CGRA::CGRA(entity::MRRGConfig mrrg_config) {
  memory_ptr_ = std::make_shared<entity::Memory>();
  *memory_ptr_ = entity::Memory();

  row_ = mrrg_config.row;
  column_ = mrrg_config.column;
  register_size_ = mrrg_config.local_reg_size;
  context_size_ = mrrg_config.context_size;

  PE_array_.resize(row_);
  for (int i = 0; i < row_; i++) {
    PE_array_[i].resize(column_);
    for (int j = 0; j < column_; j++) {
      PE_array_[i][j] = PE(register_size_, context_size_, memory_ptr_);
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
          if (adj_row_id == 0 && adj_column_id == 0) continue;

          bool is_diagonal = (abs(row_diff) + abs(column_diff)) == 2;
          if (mrrg_config.network_type ==
                  entity::MRRGNetworkType::kOrthogonal &&
              is_diagonal)
            continue;

          entity::PEPositionId adj_position_id(adj_row_id, adj_column_id);
          entity::Wire<int> new_output_wire;
          PE_array_[row_id][column_id].SetOutputWire(adj_position_id,
                                                     new_output_wire);
          entity::PEPositionId position_id(row_id, column_id);
          entity::Wire<int> adj_output_wire =
              PE_array_[adj_row_id][adj_column_id].GetOutputWire(position_id);
          PE_array_[row_id][column_id].SetInputWire(adj_position_id,
                                                    adj_output_wire);
        }
      }
    }
  }
}

void entity::CGRA::SetConfig(std::shared_ptr<entity::Mapping> mapping) {
  entity::ConfigMap config_map = mapping->GetConfigMap();
  for (auto itr = config_map.begin(); itr != config_map.end(); itr++) {
    entity::ConfigId tmp_config_id = itr->first;
    PE_array_[tmp_config_id.row_id][tmp_config_id.column_id].SetConfig(
        tmp_config_id.context_id, config_map[tmp_config_id]);
  }
}

void entity::CGRA::Update() {
  for (int row_id = 0; row_id < row_; row_id++) {
    for (int column_id = 0; column_id < column_; column_id++) {
      PE_array_[row_id][column_id].Update();
    }
  }
}

void entity::CGRA::RegisterUpdate() {
  for (int row_id = 0; row_id < row_; row_id++) {
    for (int column_id = 0; column_id < column_; column_id++) {
      PE_array_[row_id][column_id].RegisterUpdate();
    }
  }
}

void entity::CGRA::StoreMemoryData(int address, int store_data) {
  memory_ptr_->Store(address, store_data);
}