#pragma once

#include <unordered_map>

namespace simulator {
class Memory {
 public:
  Memory() : memory_map_({}){};
  int Load(int address);
  void Store(int address, int store_data);

 private:
  std::unordered_map<int, int> memory_map_;
};
}  // namespace simulator