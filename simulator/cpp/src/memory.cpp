#include <cpp_simulator/memory.hpp>

int simulator::Memory::Load(int address) {
  if (memory_map_.count(address) == 0) return 0;
  return memory_map_[address];
}

void simulator::Memory::Store(int address, int store_data) {
  if (memory_map_.count(address) == 0) {
    memory_map_.emplace(address, store_data);
  } else {
    memory_map_[address] = store_data;
  }

  return;
}
