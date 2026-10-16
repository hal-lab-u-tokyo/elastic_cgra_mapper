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
int ExecuteOperation(OpType op_type, int input1, int input2, int const_value,
                     std::vector<int>& memory);
}  // namespace entity
