#pragma once

#include <entity/architecture.hpp>
#include <simulator/elastic_module.hpp>
#include <simulator/elastic_wire.hpp>
#include <simulator/memory.hpp>
#include <simulator/module.hpp>

namespace simulator {
class ElasticPE : public IModule {
 public:
  ElasticPE(
      int buffer_size, int config_size, std::shared_ptr<Memory> memory_ptr,
      int row_id, int column_id,
      std::function<int(entity::OpType, int, int,
                        std::shared_ptr<simulator::Memory>, entity::CGRAConfig)>
          execute_operation_func);
  void SetConfig(int id, entity::CGRAConfig config);
  void SetInputWire(entity::PEPositionId position_id, ElasticWire<int> wire);
  void SetOutputWire(entity::PEPositionId position_id, ElasticWire<int> wire);
  ElasticWire<int> GetOutputWire(entity::PEPositionId position_id) {
    return output_fork_.GetOutputWire(position_id);
  }
  int GetOutput() { return output_fork_.GetOutput(); }
  void Update();
  void RegisterUpdate();

 private:
  entity::PEPositionId position_id_;
  std::shared_ptr<Memory> memory_ptr_;

  // elastic modules
  ElasticMultiPlexer<int> input_mux_1_, input_mux_2_, bypass_mux_, output_mux_;
  std::vector<ElasticFork<int>> input_fork_vec_;
  ElasticFork<int> output_fork_;
  ElasticJoin<int> join_;
  ElasticVLU<int> VLU_;
  ElasticBuffer<int> buffer_;
  ElasticRegister<int> register_;

  int buffer_size_;
  int config_size_;
};
}  // namespace simulator