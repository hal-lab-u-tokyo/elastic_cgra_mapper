#pragma once

#include <cpp_simulator/elastic_PE.hpp>
#include <cpp_simulator/elastic_module.hpp>
#include <cpp_simulator/module.hpp>
#include <entity/mapping.hpp>

namespace simulator {
class ElasticCGRA : public IModule {
 public:
  ElasticCGRA(entity::MRRGConfig mrrg_config);
  void SetConfig(std::shared_ptr<entity::Mapping> mapping);
  void Update();
  void RegisterUpdate();
  void StoreMemoryData(int address, int store_data);
  std::vector<int> GetOutputResult() { return output_vec_; };

 private:
  int row_, column_, register_size_, context_size_;
  int num_update_;
  std::vector<std::vector<ElasticPE>> PE_array_;
  std::shared_ptr<Memory> memory_ptr_;
  entity::ConfigId output_config_id_;
  std::vector<int> output_vec_;
};
}  // namespace simulator
