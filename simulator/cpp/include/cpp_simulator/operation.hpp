#pragma once

#include <cpp_simulator/memory.hpp>
#include <entity/operation.hpp>

namespace simulator {
template <typename T>
T ExecuteOperation(entity::OpType op_type, T input_1, T input_2,
                   std::shared_ptr<Memory> memory_ptr,
                   entity::CGRAConfig cgra_config) {
  T output;
  switch (op_type) {
    case entity::OpType::kAdd:
      output = input_1 + input_2;
      break;
    case entity::OpType::kSub:
      output = input_1 - input_2;
      break;
    case entity::OpType::kMul:
      output = input_1 * input_2;
      break;
    case entity::OpType::kDiv:
      output = input_1 / input_2;
      break;
    case entity::OpType::kConst:
      output = cgra_config.const_value;
      break;
    case entity::OpType::kLoad:
      output = memory_ptr->Load(input_1);
      break;
    case entity::OpType::kOutput:
      output = input_1;
      break;
    case entity::OpType::kNop:
      output = 0;
      break;
    case entity::OpType::kRoute:
      output = input_1;
      break;
    default:
      output = 0;
      break;
  }

  return output;
}
}  // namespace simulator
