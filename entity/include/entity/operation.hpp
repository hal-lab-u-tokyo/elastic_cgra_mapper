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
  kSelect
};
std::string OpTypeToString(OpType op);
OpType OpTypeFromString(std::string op_string);
std::vector<OpType> GetAllOperations();
std::vector<OpType> GetAllOperationsExceptMemoryAccess();
std::vector<OpType> GetLoopOperations();
bool IsMemoryAccessOperation(OpType op);
bool IsDFGOp(OpType op);
}  // namespace entity
