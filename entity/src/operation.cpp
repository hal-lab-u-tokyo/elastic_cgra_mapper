#include <cassert>
#include <entity/operation.hpp>

std::string entity::OpTypeToString(OpType op) {
  switch (op) {
    case entity::OpType::ADD:
      return "add";
      break;
    case entity::OpType::SUB:
      return "sub";
      break;
    case entity::OpType::MUL:
      return "mul";
      break;
    case entity::OpType::DIV:
      return "div";
      break;
    case entity::OpType::CONST:
      return "const";
      break;
    case entity::OpType::LOAD:
      return "load";
      break;
    case entity::OpType::OUTPUT:
      return "output";
      break;
    default:
      assert("invalid OpType");
      abort();
  }
};

entity::OpType entity::OpTypeFromString(std::string op_string) {
  if (op_string == "add") {
    return entity::OpType::ADD;
  } else if (op_string == "sub") {
    return entity::OpType::SUB;
  } else if (op_string == "mul") {
    return entity::OpType::MUL;
  } else if (op_string == "div") {
    return entity::OpType::DIV;
  } else if (op_string == "const") {
    return entity::OpType::CONST;
  } else if (op_string == "load") {
    return entity::OpType::LOAD;
  } else if (op_string == "output") {
    return entity::OpType::OUTPUT;
  } else {
    assert("invalid op string");
    abort();
  }
};