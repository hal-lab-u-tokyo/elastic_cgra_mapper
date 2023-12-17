#include <cassert>
#include <entity/operation.hpp>
#include <iostream>

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
    case entity::OpType::NOP:
      return "nop";
      break;
    case entity::OpType::ROUTE:
      return "route";
      break;
    case entity::OpType::STORE:
      return "store";
      break;
    case entity::OpType::OR:
      return "or";
      break;
    case entity::OpType::SHIFT:
      return "shift";
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
  } else if (op_string == "store") {
    return entity::OpType::STORE;
  } else if (op_string == "nop") {
    return entity::OpType::NOP;
  } else if (op_string == "route") {
    return entity::OpType::ROUTE;
  } else if (op_string == "or") {
    return entity::OpType::OR;
  } else if (op_string == "shift") {
    return entity::OpType::SHIFT;
  } else {
    std::string message = "invalid op string: " + op_string;
    std::cerr << message << std::endl;
    abort();
  }
};

std::vector<entity::OpType> entity::GetAllOperations() {
  return std::vector<OpType>(
      {entity::OpType::ADD, entity::OpType::SUB, entity::OpType::MUL,
       entity::OpType::DIV, entity::OpType::CONST, entity::OpType::LOAD,
       entity::OpType::OUTPUT, entity::OpType::STORE, entity::OpType::NOP,
       entity::OpType::ROUTE, entity::OpType::OR, entity::OpType::SHIFT});
};

std::vector<entity::OpType> entity::GetAllOperationsExceptMemoryAccess() {
  return std::vector<OpType>(
      {entity::OpType::ADD, entity::OpType::SUB, entity::OpType::MUL,
       entity::OpType::DIV, entity::OpType::CONST, entity::OpType::NOP,
       entity::OpType::ROUTE, entity::OpType::OR, entity::OpType::SHIFT});
};

bool entity::IsMemoryAccessOperation(OpType op) {
  return (op == entity::OpType::LOAD || op == entity::OpType::STORE ||
          op == entity::OpType::OUTPUT);
};

bool entity::IsDFGOp(OpType op) {
  return !(op == entity::OpType::NOP || op == entity::OpType::ROUTE);
};