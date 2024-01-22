#include <remapper/mapping_transform_op.hpp>

remapper::RotateOp remapper::CombineRotateOp(const remapper::RotateOp& lhs,
                                             const remapper::RotateOp& rhs) {
  auto Exec = [](remapper::RotateOp a, remapper::RotateOp b) {
    return static_cast<remapper::RotateOp>(
        (static_cast<int>(a) + static_cast<int>(b)) % 4);
  };

  auto IsRotateWithoutMirror = [](remapper::RotateOp rotate_op) {
    return static_cast<int>(rotate_op) < 4;
  };

  auto WithMirror = [](remapper::RotateOp rotate_op) {
    return static_cast<remapper::RotateOp>(static_cast<int>(rotate_op) + 4);
  };

  auto WithoutMirror = [](remapper::RotateOp rotate_op) {
    return static_cast<remapper::RotateOp>(static_cast<int>(rotate_op) - 4);
  };

  auto RotateWithMirror = [](remapper::RotateOp rotate_op) {
    return static_cast<remapper::RotateOp>((static_cast<int>(rotate_op) + 2) %
                                           4);
  };

  if (IsRotateWithoutMirror(lhs) && IsRotateWithoutMirror(rhs)) {
    return Exec(lhs, rhs);
  } else if (IsRotateWithoutMirror(lhs) && !IsRotateWithoutMirror(rhs)) {
    return WithMirror(Exec(lhs, WithoutMirror(rhs)));
  } else if (!IsRotateWithoutMirror(lhs) && IsRotateWithoutMirror(rhs)) {
    return WithMirror(Exec(WithoutMirror(lhs), RotateWithMirror(rhs)));
  } else {
    return Exec(WithoutMirror(lhs), RotateWithMirror(WithoutMirror(rhs)));
  }
}