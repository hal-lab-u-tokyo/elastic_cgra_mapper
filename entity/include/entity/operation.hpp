#pragma once

#include <string>
#include <vector>

namespace entity {
enum class OpType {
  kAdd,
  kFAdd,
  kSub,
  kMul,
  kFMul,
  kDiv,
  kSDiv,
  kFDiv,
  kConst,
  kLoad,
  kOutput,
  kStore,
  kNop,
  kRoute,
  kOr,
  kShift,
  kIcmp,
  kCmpGt,
  kCmpGe,
  kCmpEq,
  kFSub,
  kLoop,
  kSelect,
  // PR131 mapper spellings retained as aliases to the current scoped enum.
  ADD = kAdd,
  FADD = kFAdd,
  SUB = kSub,
  MUL = kMul,
  FMUL = kFMul,
  DIV = kDiv,
  SDIV = kSDiv,
  FDIV = kFDiv,
  CONST = kConst,
  LOAD = kLoad,
  OUTPUT = kOutput,
  STORE = kStore,
  NOP = kNop,
  ROUTE = kRoute,
  OR = kOr,
  SHIFT = kShift,
  ICMP = kIcmp,
  CMPGT = kCmpGt,
  CMPGE = kCmpGe,
  CMPEQ = kCmpEq,
  FSUB = kFSub,
  LOOP = kLoop,
  SELECT = kSelect
};
std::string OpTypeToString(OpType op);
OpType OpTypeFromString(std::string op_string);
std::vector<OpType> GetAllOperations();
std::vector<OpType> GetAllOperationsExceptMemoryAccess();
std::vector<OpType> GetLoopOperations();
bool IsMemoryAccessOperation(OpType op);
bool IsDFGOp(OpType op);
int ExecuteOperation(OpType op_type, int input1, int input2, int const_value,
                     std::vector<int>& memory);
}  // namespace entity
