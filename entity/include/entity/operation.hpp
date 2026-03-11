#pragma once

#include <string>
#include <vector>

namespace entity {
enum OpType {
  ADD,
  SUB,
  MUL,
  DIV,
  CONST,
  LOAD,
  OUTPUT,
  STORE,
  NOP,
  ROUTE,
  OR,
  LOOP,
};
std::string OpTypeToString(OpType op);
OpType OpTypeFromString(std::string op_string);
std::vector<OpType> GetAllOperations();
std::vector<OpType> GetAllOperationsExceptMemoryAccess();
std::vector<OpType> GetLoopOperations();
bool IsMemoryAccessOperation(OpType op);
bool IsDFGOp(OpType op);
}  // namespace entity
