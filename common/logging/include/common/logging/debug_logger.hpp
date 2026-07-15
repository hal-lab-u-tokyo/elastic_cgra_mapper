#pragma once

#include <iostream>

namespace common::logging {

class DebugLogger {
 public:
  explicit DebugLogger(bool enabled) : enabled_(enabled) {}

  template <typename T>
  DebugLogger& operator<<(const T& value) {
    if (enabled_) std::cout << value;
    return *this;
  }

  using StreamManipulator = std::ostream& (*)(std::ostream&);
  DebugLogger& operator<<(StreamManipulator manipulator) {
    if (enabled_) manipulator(std::cout);
    return *this;
  }

 private:
  bool enabled_;
};

}  // namespace common::logging
