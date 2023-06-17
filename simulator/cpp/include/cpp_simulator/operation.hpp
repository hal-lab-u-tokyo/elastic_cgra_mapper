#pragma once

#include <entity/operation.hpp>
#include <cpp_simulator/memory.hpp>

namespace simulator {
template <typename T>
T ExecuteOperation(entity::OpType op_type, T input_1, T input_2,
                   std::shared_ptr<Memory> memory_ptr,
                   entity::CGRAConfig cgra_config) {
  T output;
  switch (op_type) {
    case entity::OpType::ADD:
      output = input_1 + input_2;
      break;
    case entity::OpType::SUB:
      output = input_1 - input_2;
      break;
    case entity::OpType::MUL:
      output = input_1 * input_2;
      break;
    case entity::OpType::DIV:
      output = input_1 / input_2;
      break;
    case entity::OpType::CONST:
      output = cgra_config.const_value;
      break;
    case entity::OpType::LOAD:
      output = memory_ptr->Load(input_1);
      break;
    case entity::OpType::OUTPUT:
      output = input_1;
      break;
    case entity::OpType::NOP:
      output = 0;
      break;
    case entity::OpType::ROUTE:
      output = input_1;
      break;
    default:
      output = 0;
      break;
  }

  return output;
}
}  // namespace simulator