#pragma once

namespace entity {
template <typename T>
class Wire {
 public:
  void UpdateValue(T value) { *value_ = value; };
  T GetValue() { return *(value_); };

 private:
  std::shared_ptr<T> value_;
};
}  // namespace entity