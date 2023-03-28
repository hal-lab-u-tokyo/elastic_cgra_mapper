#include <entity/operation.hpp>
#include <simulator/PE.hpp>
#include <simulator/operation.hpp>

simulator::PE::PE() : input_wire_({}), output_wire_({}), position_id_(0, 0) {
  register_size_ = 1;
  config_size_ = 1;
  register_.resize(register_size_);
  config_.resize(config_size_);
  tmp_config_id_ = 0;
  output_ = 0;

  memory_ptr_ = std::make_shared<simulator::Memory>();
  position_id_ = entity::PEPositionId(0, 0);
}

simulator::PE::PE(int register_size, int config_size,
                  std::shared_ptr<simulator::Memory> memory_ptr, int row_id,
                  int column_id)
    : input_wire_({}),
      output_wire_({}),
      memory_ptr_(memory_ptr),
      position_id_(row_id, column_id),
      register_size_(register_size),
      config_size_(config_size) {
  register_.resize(register_size);
  config_.resize(config_size);
  tmp_config_id_ = 0;
  output_ = 0;
}

void simulator::PE::SetConfig(int id, entity::CGRAConfig config) {
  config_[id] = config;
}

void simulator::PE::SetInputWire(entity::PEPositionId position_id,
                                 simulator::Wire<int> wire) {
  if (input_wire_.count(position_id) > 0) return;
  input_wire_.emplace(position_id, wire);
}

void simulator::PE::SetOutputWire(entity::PEPositionId position_id,
                                  simulator::Wire<int> wire) {
  if (output_wire_.count(position_id) > 0) return;
  output_wire_.emplace(position_id, wire);
}

simulator::Wire<int> simulator::PE::GetOutputWire(
    entity::PEPositionId position_id) {
  if (output_wire_.count(position_id) == 0) {
    simulator::Wire<int> new_wire;
    output_wire_.emplace(position_id, new_wire);
  }

  return output_wire_[position_id];
}

void simulator::PE::Update() {
  entity::CGRAConfig tmp_config = config_[tmp_config_id_];
  int input_1, input_2;

  auto GetWireValue = [&](entity::PEPositionId position_id) {
    if (position_id == position_id_) {
      return output_;
    } else {
      return input_wire_[position_id].GetValue();
    }
  };

  input_1 = GetWireValue(tmp_config.from_config_id_vec[0].GetPositionId());
  input_2 = GetWireValue(tmp_config.from_config_id_vec[1].GetPositionId());

  // execute operation
  output_ = simulator::ExecuteOperation(tmp_config.operation_type, input_1,
                                        input_2, memory_ptr_, tmp_config);

  if (tmp_config.operation_name != "") {
    std::cout << tmp_config.operation_name << std::endl;
    std::cout << output_ << std::endl;
  }
  // update config id
  tmp_config_id_ = (tmp_config_id_ + 1) % config_size_;
}

void simulator::PE::RegisterUpdate() {
  // output wire update (output register)
  for (auto itr = output_wire_.begin(); itr != output_wire_.end(); itr++) {
    itr->second.UpdateValue(output_);
  }

  // update local register
  for (int i = register_size_ - 2; i >= 0; i--) {
    register_[i + 1] = register_[i];
  }
  register_[0] = output_;
}