#pragma once

namespace remapper {
enum RotateOp { TopIsTop, TopIsRight, TopIsBottom, TopIsLeft };

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