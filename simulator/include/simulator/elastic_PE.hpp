#pragma once

#include <entity/architecture.hpp>
#include <simulator/elastic_module.hpp>
#include <simulator/elastic_wire.hpp>
#include <simulator/memory.hpp>

namespace simulator {
class ElasticPE {
 public:
  ElasticPE(
      int buffer_size, int config_size, std::shared_ptr<Memory> memory_ptr,
      int row_id, int column_id,
      std::function<int(entity::OpType, int, int)> execute_operation_func);
  void SetInputWire(entity::PEPositionId position_id, ElasticWire<int> wire);
  void SetOutputWire(entity::PEPositionId position_id, ElasticWire<int> wire);

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
  std::vector<ElasticWire<int>> elastic_wire_vec_;
  int buffer_size_;
  int config_size_;

  const entity::PEPositionId bypass_position_ = entity::PEPositionId(-1, -1);
  const entity::PEPositionId buffer_position_ = entity::PEPositionId(-2, -2);
};
}  // namespace simulator