#include <entity/operation.hpp>
#include <simulator/PE.hpp>

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
      register_size_(register_size),
      config_size_(config_size),
      memory_ptr_(memory_ptr),
      position_id_(row_id, column_id) {
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

  // if (input_wire_.count(entity::PEPositionId(0, 0)) >= 1) {
  //   std::cout << "input:" << input_wire_[entity::PEPositionId(0,
  //   0)]
  //             << std::endl;
  // }
  // if (output_wire_.count(entity::PEPositionId(0, 0)) >= 1) {
  //   std::cout << "output:"
  //             << output_wire_[entity::PEPositionId(0, 0)]
  //             << std::endl;
  // }
  auto GetWireValue = [&](entity::PEPositionId position_id) {
    if (position_id == position_id_) {
      return output_;
    } else {
      return input_wire_[position_id].GetValue();
    }
  };

  // execute operation
  switch (tmp_config.operation_type) {
    case entity::OpType::ADD:
      input_1 = GetWireValue(tmp_config.from_config_id_vec[0].GetPositionId());
      input_2 = GetWireValue(tmp_config.from_config_id_vec[1].GetPositionId());
      // if (tmp_config.operation_name == "add15") {
      //   std::cout << tmp_config.from_config_id_vec[0].row_id << ","
      //             << tmp_config.from_config_id_vec[0].column_id << ","
      //             << tmp_config.from_config_id_vec[0].context_id <<
      //             std::endl;
      //   std::cout << tmp_config.from_config_id_vec[1].row_id << ","
      //             << tmp_config.from_config_id_vec[1].column_id << ","
      //             << tmp_config.from_config_id_vec[1].context_id <<
      //             std::endl;
      //   std::cout << input_1 << ":" << input_2
      //             << std::endl;
      // }
      output_ = input_1 + input_2;
      break;
    case entity::OpType::SUB:
      input_1 = GetWireValue(tmp_config.from_config_id_vec[0].GetPositionId());
      input_2 = GetWireValue(tmp_config.from_config_id_vec[1].GetPositionId());
      output_ = input_1 - input_2;
      break;
    case entity::OpType::MUL:
      input_1 = GetWireValue(tmp_config.from_config_id_vec[0].GetPositionId());
      input_2 = GetWireValue(tmp_config.from_config_id_vec[1].GetPositionId());
      std::cout << input_1 << ":" << input_2 << std::endl;
      output_ = input_1 * input_2;
      break;
    case entity::OpType::DIV:
      input_1 = GetWireValue(tmp_config.from_config_id_vec[0].GetPositionId());
      input_2 = GetWireValue(tmp_config.from_config_id_vec[1].GetPositionId());
      output_ = input_1 / input_2;
      break;
    case entity::OpType::CONST:
      // if (tmp_config.operation_name == "const16") {
      //   std::cout << tmp_config.const_value << std::endl;
      // }
      output_ = tmp_config.const_value;
      break;
    case entity::OpType::LOAD:
      input_1 = GetWireValue(tmp_config.from_config_id_vec[0].GetPositionId());
      output_ = memory_ptr_->Load(input_1);
      break;
    case entity::OpType::OUTPUT:
      input_1 = GetWireValue(tmp_config.from_config_id_vec[0].GetPositionId());
      output_ = input_1;
      break;
    case entity::OpType::NOP:
      output_ = 0;
      break;
    case entity::OpType::ROUTE:
      input_1 = GetWireValue(tmp_config.from_config_id_vec[0].GetPositionId());
      // std::cout << "route" << std::endl;
      // std::cout << tmp_config.from_config_id_vec[0].row_id << ","
      //           << tmp_config.from_config_id_vec[0].column_id << ","
      //           << tmp_config.from_config_id_vec[0].context_id << std::endl;
      // std::cout << output_ << "->" << input_1 << std::endl;
      output_ = input_1;
      break;
    default:
      output_ = 0;
      break;
  }
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