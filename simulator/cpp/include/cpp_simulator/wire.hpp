#pragma once

namespace simulator {
template <typename T>
class Wire {
 public:
  Wire() { value_ = std::make_shared<T>(); }
  void UpdateValue(T value) { *value_ = value; };
  T GetValue() { return *(value_); };

 private:
  std::shared_ptr<T> value_;
};
}  // namespace simulator
