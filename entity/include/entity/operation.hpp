#pragma once

#include <string>
#include <vector>

namespace entity {
enum OpType { ADD, SUB, MUL, DIV, CONST, LOAD, OUTPUT, NOP };
std::string OpTypeToString(OpType op);
OpType OpTypeFromString(std::string op_string);
std::vector<OpType> GetAllOperations(); 
}  // namespace entity