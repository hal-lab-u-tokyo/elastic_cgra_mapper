#include <simulator/memory.hpp>

int entity::Memory::Load(int address) {
    return memory_map_[address];
}

void entity::Memory::Store(int address, int store_data) {
    if(memory_map_.count(address) == 0) {
        memory_map_.emplace(address, store_data);
    } else {
        memory_map_[address] = store_data;
    }

    return;
}