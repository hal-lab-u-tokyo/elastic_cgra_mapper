#pragma once

#include <string>
#include <vector>

namespace entity {
enum OpType { ADD, SUB, MUL, DIV, CONST, LOAD, OUTPUT, NOP, ROUTE };
std::string OpTypeToString(OpType op);
OpType OpTypeFromString(std::string op_string);
std::vector<OpType> GetAllOperations();
std::vector<OpType> GetAllOperationsExceptMemoryAccess();
}  // namespace entity