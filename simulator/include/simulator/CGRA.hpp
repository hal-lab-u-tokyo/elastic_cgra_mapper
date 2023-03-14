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
  std::vector<int> GetOutputResult() { return output_vec_; };

 private:
  int row_, column_, register_size_, context_size_;
  int num_update_;
  std::vector<std::vector<PE>> PE_array_;
  std::shared_ptr<Memory> memory_ptr_;
  entity::ConfigId output_config_id_;
  std::vector<int> output_vec_;
};
}  // namespace simulator