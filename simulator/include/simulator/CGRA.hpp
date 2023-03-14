#pragma once

#include <simulator/PE.hpp>
#include <entity/mapping.hpp>
#include <simulator/module.hpp>

namespace entity {
class CGRA : public IModule {
 public:
  CGRA(MRRGConfig mrrg_config);
  void SetConfig(std::shared_ptr<Mapping> mapping);
  void Update();
  void RegisterUpdate();
  void StoreMemoryData(int address, int store_data);

 private:
  int row_, column_, register_size_, context_size_;
  std::vector<std::vector<PE>> PE_array_;
  std::shared_ptr<Memory> memory_ptr_;
};
}  // namespace entity