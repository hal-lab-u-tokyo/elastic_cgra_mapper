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
  OR,
  AND,
  XOR,
  SHIFTL,
  SHIFTR,
  ICMP,
  CMPGT,
  CMPGE,
  CMPEQ,
  FSUB,
  LOOP,
  TM,
  SELECT
};
std::string OpTypeToString(OpType op);
OpType OpTypeFromString(std::string op_string);
std::vector<OpType> GetAllOperations();
std::vector<OpType> GetAllOperationsExceptMemoryAccess();
std::vector<OpType> GetLoopOperations();
std::vector<OpType> GetTMOperations();
bool IsMemoryAccessOperation(OpType op);
bool IsDFGOp(OpType op);
}  // namespace entity
