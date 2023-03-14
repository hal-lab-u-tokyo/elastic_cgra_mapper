#pragma once

#include <entity/architecture.hpp>
#include <simulator/memory.hpp>
#include <simulator/module.hpp>
#include <simulator/wire.hpp>
#include <vector>

namespace entity {
class PE : public IModule {
 public:
  PE();
  PE(int register_size, int config_size, std::shared_ptr<Memory> memory_ptr);
  void SetConfig(int id, CGRAConfig config);
  void SetInputWire(PEPositionId position_id, Wire<int> wire);
  void SetOutputWire(PEPositionId position_id, Wire<int> wire);
  Wire<int> GetOutputWire(PEPositionId position_id);
  void Update();
  void RegisterUpdate();

 private:
  std::unordered_map<PEPositionId, Wire<int>, HashPEPositionId> input_wire_;
  std::unordered_map<PEPositionId, Wire<int>, HashPEPositionId> output_wire_;
  std::vector<CGRAConfig> config_;
  std::vector<int> register_;
  int register_size_;
  int config_size_;
  int tmp_config_id_;
  int output_;
  std::shared_ptr<Memory> memory_ptr_;
};
}  // namespace entity