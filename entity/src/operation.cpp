#include <cassert>
#include <entity/operation.hpp>
#include <iostream>

std::string entity::OpTypeToString(OpType op) {
  switch (op) {
    case entity::OpType::ADD:
      return "add";
      break;
    case entity::OpType::FADD:
      return "fadd";
      break;
    case entity::OpType::SUB:
      return "sub";
      break;
    case entity::OpType::FSUB:
      return "fsub";
      break;
    case entity::OpType::MUL:
      return "mul";
      break;
    case entity::OpType::FMUL:
      return "fmul";
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
    case entity::OpType::ICMP:
      return "icmp";
      break;
    case entity::OpType::CMPGT:
      return "cmpgt";
      break;
    case entity::OpType::CMPGE:
      return "cmpge";
      break;
    case entity::OpType::CMPEQ:
      return "cmpeq";
      break;
    case entity::OpType::SDIV:
      return "sdiv";
      break;
    case entity::OpType::FDIV:
      return "fdiv";
      break;
    case entity::OpType::LOOP:
      return "loop";
      break;
    case entity::OpType::SELECT:
      return "select";
    default:
      assert("invalid OpType");
      abort();
  }
};

entity::OpType entity::OpTypeFromString(std::string op_string) {
  if (op_string == "add") {
    return entity::OpType::ADD;
  } else if (op_string == "fadd") {
    return entity::OpType::FADD;
  } else if (op_string == "sub") {
    return entity::OpType::SUB;
  } else if (op_string == "fsub") {
    return entity::OpType::FSUB;
  } else if (op_string == "mul") {
    return entity::OpType::MUL;
  } else if (op_string == "fmul") {
    return entity::OpType::FMUL;
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
  } else if (op_string == "icmp") {
    return entity::OpType::ICMP;
  } else if (op_string == "cmpgt" || op_string == "icmpgt") {
    return entity::OpType::CMPGT;
  } else if (op_string == "cmpge" || op_string == "icmpge") {
    return entity::OpType::CMPGE;
  } else if (op_string == "cmpeq" || op_string == "icmpeq") {
    return entity::OpType::CMPEQ;
  } else if (op_string == "sdiv") {
    return entity::OpType::SDIV;
  } else if (op_string == "fdiv") {
    return entity::OpType::FDIV;
  } else if (op_string == "loop") {
    return entity::OpType::LOOP;
  } else if (op_string == "select") {
    return entity::OpType::SELECT;
  } else {
    std::string message = "invalid op string: " + op_string;
    std::cerr << message << std::endl;
    abort();
  }
};

std::vector<entity::OpType> entity::GetAllOperations() {
  return std::vector<OpType>(
      {entity::OpType::ADD, entity::OpType::FADD, entity::OpType::SUB, 
       entity::OpType::FSUB, entity::OpType::MUL, entity::OpType::FMUL,
       entity::OpType::DIV, entity::OpType::SDIV, entity::OpType::FDIV, 
       entity::OpType::CONST, entity::OpType::LOAD, entity::OpType::OUTPUT,
       entity::OpType::STORE, entity::OpType::NOP, entity::OpType::ROUTE, 
       entity::OpType::OR, entity::OpType::SHIFT, entity::OpType::ICMP, entity::OpType::CMPGT,   
       entity::OpType::CMPGE, entity::OpType::CMPEQ, entity::OpType::SELECT});
};

std::vector<entity::OpType> entity::GetAllOperationsExceptMemoryAccess() {
  return std::vector<OpType>(
      {entity::OpType::ADD, entity::OpType::FADD, entity::OpType::SUB, 
       entity::OpType::FSUB, entity::OpType::MUL, entity::OpType::FMUL,
       entity::OpType::DIV, entity::OpType::SDIV, entity::OpType::FDIV,
       entity::OpType::CONST, entity::OpType::NOP,entity::OpType::ROUTE, 
       entity::OpType::OR, entity::OpType::SHIFT, entity::OpType::ICMP, 
      entity::OpType::CMPGT, entity::OpType::CMPGE, entity::OpType::CMPEQ,
       entity::OpType::SELECT});
};

std::vector<entity::OpType> entity::GetLoopOperations() {
  return std::vector<OpType>(
      {entity::OpType::LOOP});
};

bool entity::IsMemoryAccessOperation(OpType op) {
  return (op == entity::OpType::LOAD || op == entity::OpType::STORE ||
          op == entity::OpType::OUTPUT);
};

bool entity::IsDFGOp(OpType op) {
  return !(op == entity::OpType::NOP || op == entity::OpType::ROUTE);
};
