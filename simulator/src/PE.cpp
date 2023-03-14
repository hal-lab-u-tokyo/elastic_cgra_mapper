#include <entity/operation.hpp>
#include <simulator/PE.hpp>

entity::PE::PE() : input_wire_({}), output_wire_({}) {
  register_size_ = 1;
  config_size_ = 1;
  register_.resize(register_size_);
  config_.resize(config_size_);
  tmp_config_id_ = 0;
  output_ = 0;

  memory_ptr_ = std::make_shared<entity::Memory>();
}

entity::PE::PE(int register_size, int config_size,
               std::shared_ptr<entity::Memory> memory_ptr)
    : input_wire_({}),
      output_wire_({}),
      register_size_(register_size),
      config_size_(config_size),
      memory_ptr_(memory_ptr) {
  register_.resize(register_size);
  config_.resize(config_size);
  tmp_config_id_ = 0;
  output_ = 0;
}

void entity::PE::SetConfig(int id, CGRAConfig config) { config_[id] = config; }

void entity::PE::SetInputWire(entity::PEPositionId position_id,
                              Wire<int> wire) {
  input_wire_[position_id] = wire;
}

void entity::PE::SetOutputWire(entity::PEPositionId position_id,
                               Wire<int> wire) {
  if (output_wire_.count(position_id) > 0) return;
  output_wire_[position_id] = wire;
}

entity::Wire<int> entity::PE::GetOutputWire(PEPositionId position_id) {
  if (output_wire_.count(position_id) == 0) {
    entity::Wire<int> new_wire;
    output_wire_.emplace(position_id, new_wire);
  }

  return output_wire_[position_id];
}

void entity::PE::Update() {
  entity::CGRAConfig tmp_config = config_[tmp_config_id_];
  output_ = 0;
  entity::Wire<int> input_1, input_2;

  // execute operation
  switch (tmp_config.operation_type) {
    case entity::OpType::ADD:
      input_1 = input_wire_[tmp_config.from_config_id_vec[0].GetPositionId()];
      input_2 = input_wire_[tmp_config.from_config_id_vec[1].GetPositionId()];
      output_ = input_1.GetValue() + input_2.GetValue();
      break;
    case entity::OpType::SUB:
      input_1 = input_wire_[tmp_config.from_config_id_vec[0].GetPositionId()];
      input_2 = input_wire_[tmp_config.from_config_id_vec[1].GetPositionId()];
      output_ = input_1.GetValue() - input_2.GetValue();
      break;
    case entity::OpType::MUL:
      input_1 = input_wire_[tmp_config.from_config_id_vec[0].GetPositionId()];
      input_2 = input_wire_[tmp_config.from_config_id_vec[1].GetPositionId()];
      output_ = input_1.GetValue() * input_2.GetValue();
      break;
    case entity::OpType::DIV:
      input_1 = input_wire_[tmp_config.from_config_id_vec[0].GetPositionId()];
      input_2 = input_wire_[tmp_config.from_config_id_vec[1].GetPositionId()];
      output_ = input_1.GetValue() / input_2.GetValue();
      break;
    case entity::OpType::CONST:
      output_ = tmp_config.const_value;
      break;
    case entity::OpType::LOAD:
      input_1 = input_wire_[tmp_config.from_config_id_vec[0].GetPositionId()];
      output_ = memory_ptr_->Load(input_1.GetValue());
      break;
    case entity::OpType::OUTPUT:
      input_1 = input_wire_[tmp_config.from_config_id_vec[0].GetPositionId()];
      input_2 = input_wire_[tmp_config.from_config_id_vec[1].GetPositionId()];
      memory_ptr_->Store(input_1.GetValue(), input_2.GetValue());
      break;
    case entity::OpType::NOP:
      output_ = 0;
      break;
    case entity::OpType::ROUTE:
      input_1 = input_wire_[tmp_config.from_config_id_vec[0].GetPositionId()];
      output_ = input_1.GetValue();
      break;
    default:
      break;
  }

  // update config id
  tmp_config_id_ = (tmp_config_id_ + 1) % config_size_;
}

void entity::PE::RegisterUpdate() {
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