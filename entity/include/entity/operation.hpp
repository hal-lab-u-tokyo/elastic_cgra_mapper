#pragma once

#include <string>
#include <vector>

namespace entity {
enum OpType {
  ADD,
  FADD,
  SUB,
  MUL,
  FMUL,
  DIV,
  SDIV,
  FDIV,
  CONST,
  LOAD,
  OUTPUT,
  STORE,
  NOP,
  ROUTE,
  AND,
  OR,
  XOR,
  SHIFT,
  ICMP,
  CMPGT,
  CMPGE,
  CMPEQ,
  FSUB,
  LOOP,
  SELECT
};
std::string OpTypeToString(OpType op);
OpType OpTypeFromString(std::string op_string);
std::vector<OpType> GetAllOperations();
std::vector<OpType> GetAllOperationsExceptMemoryAccess();
std::vector<OpType> GetLoopOperations();
bool IsMemoryAccessOperation(OpType op);
bool IsDFGOp(OpType op);
}  // namespace entity
