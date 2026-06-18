#include <cassert>
#include <entity/operation.hpp>
#include <iostream>

std::string entity::OpTypeToString(OpType op) {
  switch (op) {
    case entity::OpType::kAdd:
      return "add";
      break;
    case entity::OpType::kFAdd:
      return "fadd";
      break;
    case entity::OpType::kSub:
      return "sub";
      break;
    case entity::OpType::kFSub:
      return "fsub";
      break;
    case entity::OpType::kMul:
      return "mul";
      break;
    case entity::OpType::kFMul:
      return "fmul";
      break;
    case entity::OpType::kDiv:
      return "div";
      break;
    case entity::OpType::kConst:
      return "const";
      break;
    case entity::OpType::kLoad:
      return "load";
      break;
    case entity::OpType::kOutput:
      return "output";
      break;
    case entity::OpType::kNop:
      return "nop";
      break;
    case entity::OpType::kRoute:
      return "route";
      break;
    case entity::OpType::kStore:
      return "store";
      break;
    case entity::OpType::kOr:
      return "or";
      break;
    case entity::OpType::kShift:
      return "shift";
      break;
    case entity::OpType::kIcmp:
      return "icmp";
      break;
    case entity::OpType::kCmpGt:
      return "cmpgt";
      break;
    case entity::OpType::kCmpGe:
      return "cmpge";
      break;
    case entity::OpType::kCmpEq:
      return "cmpeq";
      break;
    case entity::OpType::kSDiv:
      return "sdiv";
      break;
    case entity::OpType::kFDiv:
      return "fdiv";
      break;
    case entity::OpType::kLoop:
      return "loop";
      break;
    case entity::OpType::kSelect:
      return "select";
    default:
      assert("invalid OpType");
      abort();
  }
};

entity::OpType entity::OpTypeFromString(std::string op_string) {
  if (op_string == "add") {
    return entity::OpType::kAdd;
  } else if (op_string == "fadd") {
    return entity::OpType::kFAdd;
  } else if (op_string == "sub") {
    return entity::OpType::kSub;
  } else if (op_string == "fsub") {
    return entity::OpType::kFSub;
  } else if (op_string == "mul") {
    return entity::OpType::kMul;
  } else if (op_string == "fmul") {
    return entity::OpType::kFMul;
  } else if (op_string == "div") {
    return entity::OpType::kDiv;
  } else if (op_string == "const") {
    return entity::OpType::kConst;
  } else if (op_string == "load") {
    return entity::OpType::kLoad;
  } else if (op_string == "output") {
    return entity::OpType::kOutput;
  } else if (op_string == "store") {
    return entity::OpType::kStore;
  } else if (op_string == "nop") {
    return entity::OpType::kNop;
  } else if (op_string == "route") {
    return entity::OpType::kRoute;
  } else if (op_string == "or") {
    return entity::OpType::kOr;
  } else if (op_string == "shift") {
    return entity::OpType::kShift;
  } else if (op_string == "icmp") {
    return entity::OpType::kIcmp;
  } else if (op_string == "cmpgt" || op_string == "icmpgt") {
    return entity::OpType::kCmpGt;
  } else if (op_string == "cmpge" || op_string == "icmpge") {
    return entity::OpType::kCmpGe;
  } else if (op_string == "cmpeq" || op_string == "icmpeq") {
    return entity::OpType::kCmpEq;
  } else if (op_string == "sdiv") {
    return entity::OpType::kSDiv;
  } else if (op_string == "fdiv") {
    return entity::OpType::kFDiv;
  } else if (op_string == "loop") {
    return entity::OpType::kLoop;
  } else if (op_string == "select") {
    return entity::OpType::kSelect;
  } else {
    std::string message = "invalid op string: " + op_string;
    std::cerr << message << std::endl;
    abort();
  }
};

std::vector<entity::OpType> entity::GetAllOperations() {
  return std::vector<OpType>(
      {entity::OpType::kAdd,   entity::OpType::kFAdd,  entity::OpType::kSub,
       entity::OpType::kFSub,  entity::OpType::kMul,   entity::OpType::kFMul,
       entity::OpType::kDiv,   entity::OpType::kSDiv,  entity::OpType::kFDiv,
       entity::OpType::kConst, entity::OpType::kLoad,  entity::OpType::kOutput,
       entity::OpType::kStore, entity::OpType::kNop,   entity::OpType::kRoute,
       entity::OpType::kOr,    entity::OpType::kShift, entity::OpType::kIcmp,
       entity::OpType::kCmpGt, entity::OpType::kCmpGe, entity::OpType::kCmpEq,
       entity::OpType::kSelect});
};

std::vector<entity::OpType> entity::GetAllOperationsExceptMemoryAccess() {
  return std::vector<OpType>(
      {entity::OpType::kAdd, entity::OpType::kFAdd, entity::OpType::kSub,
       entity::OpType::kFSub, entity::OpType::kMul, entity::OpType::kFMul,
       entity::OpType::kDiv, entity::OpType::kSDiv, entity::OpType::kFDiv,
       entity::OpType::kConst, entity::OpType::kNop, entity::OpType::kRoute,
       entity::OpType::kOr, entity::OpType::kShift, entity::OpType::kIcmp,
       entity::OpType::kCmpGt, entity::OpType::kCmpGe, entity::OpType::kCmpEq,
       entity::OpType::kSelect});
};

std::vector<entity::OpType> entity::GetLoopOperations() {
  return std::vector<OpType>({entity::OpType::kLoop});
};

bool entity::IsMemoryAccessOperation(OpType op) {
  return (op == entity::OpType::kLoad || op == entity::OpType::kStore ||
          op == entity::OpType::kOutput);
};

bool entity::IsDFGOp(OpType op) {
  return !(op == entity::OpType::kNop || op == entity::OpType::kRoute);
};
