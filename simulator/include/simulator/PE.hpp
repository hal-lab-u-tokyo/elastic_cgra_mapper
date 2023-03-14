#pragma once

#include <entity/architecture.hpp>
#include <simulator/memory.hpp>
#include <simulator/module.hpp>
#include <simulator/wire.hpp>
#include <vector>

namespace simulator {
class PE : public IModule {
 public:
  PE();
  PE(int register_size, int config_size, std::shared_ptr<Memory> memory_ptr,
     int row_id, int column_id);
  void SetConfig(int id, entity::CGRAConfig config);
  void SetInputWire(entity::PEPositionId position_id, Wire<int> wire);
  void SetOutputWire(entity::PEPositionId position_id, Wire<int> wire);
  Wire<int> GetOutputWire(entity::PEPositionId position_id);
  int GetOutput() { return output_; };
  void Update();
  void RegisterUpdate();

 private:
  std::unordered_map<entity::PEPositionId, Wire<int>, entity::HashPEPositionId>
      input_wire_;
  std::unordered_map<entity::PEPositionId, Wire<int>, entity::HashPEPositionId>
      output_wire_;
  std::vector<entity::CGRAConfig> config_;
  std::vector<int> register_;
  std::shared_ptr<Memory> memory_ptr_;
  entity::PEPositionId position_id_;
  int register_size_;
  int config_size_;
  int tmp_config_id_;
  int output_;
};
}  // namespace simulator