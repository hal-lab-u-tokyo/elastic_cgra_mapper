#pragma once
#include <vector>

namespace remapper {
enum RotateOp {
  TopIsTop = 0,
  TopIsRight = 1,
  TopIsBottom = 2,
  TopIsLeft = 3,
  TopIsTopMirror = 4,
  TopIsRightMirror = 5,
  TopIsBottomMirror = 6,
  TopIsLeftMirror = 7
};

RotateOp CombineRotateOp(const RotateOp& lhs, const RotateOp& rhs);

constexpr int kRotateOpNum = 8;

const std::vector<RotateOp> kAllRotateOpVec = {
    RotateOp::TopIsTop,          RotateOp::TopIsRight,
    RotateOp::TopIsBottom,       RotateOp::TopIsLeft,
    RotateOp::TopIsTopMirror,    RotateOp::TopIsRightMirror,
    RotateOp::TopIsBottomMirror, RotateOp::TopIsLeftMirror};
const std::vector<RotateOp> kAllRotateOpVecWithoutMirror = {
    RotateOp::TopIsTop, RotateOp::TopIsRight, RotateOp::TopIsBottom,
    RotateOp::TopIsLeft};

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
