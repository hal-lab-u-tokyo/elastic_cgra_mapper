#pragma once

#include <entity/mapping.hpp>
#include <simulator/PE.hpp>
#include <simulator/module.hpp>

namespace simulator {
class CGRA : public IModule {
 public:
  CGRA(entity::MRRGConfig mrrg_config);
  void SetConfig(std::shared_ptr<entity::Mapping> mapping);
  void Update();
  void RegisterUpdate();
  void StoreMemoryData(int address, int store_data);

 private:
  int row_, column_, register_size_, context_size_;
  std::vector<std::vector<PE>> PE_array_;
  std::shared_ptr<Memory> memory_ptr_;
};
}  // namespace simulator