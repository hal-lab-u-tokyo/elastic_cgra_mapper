#pragma once
#include <string>
#include <vector>

namespace remapper {
enum class RotateOp {
  kTopIsTop = 0,
  kTopIsRight = 1,
  kTopIsBottom = 2,
  kTopIsLeft = 3,
  kTopIsTopMirror = 4,
  kTopIsRightMirror = 5,
  kTopIsBottomMirror = 6,
  kTopIsLeftMirror = 7
};

std::string RotateOpToString(RotateOp rotate_op);

RotateOp CombineRotateOp(const RotateOp& lhs, const RotateOp& rhs);

constexpr int kRotateOpNum = 8;

enum class RotateType { kAll = 0, kWithoutMirror = 1 };

const std::vector<RotateOp> kAllRotateOpVec = {
    RotateOp::kTopIsTop,          RotateOp::kTopIsRight,
    RotateOp::kTopIsBottom,       RotateOp::kTopIsLeft,
    RotateOp::kTopIsTopMirror,    RotateOp::kTopIsRightMirror,
    RotateOp::kTopIsBottomMirror, RotateOp::kTopIsLeftMirror};
const std::vector<RotateOp> kAllRotateOpVecWithoutMirror = {
    RotateOp::kTopIsTop, RotateOp::kTopIsRight, RotateOp::kTopIsBottom,
    RotateOp::kTopIsLeft};

struct MappingTransformOp {
  MappingTransformOp()
      : row(0), column(0), rotate_op(static_cast<RotateOp>(0)){};
  MappingTransformOp(int _row, int _column, RotateOp _rotate_op)
      : row(_row), column(_column), rotate_op(_rotate_op){};

  int row;
  int column;
  RotateOp rotate_op;
};
}  // namespace remapper
