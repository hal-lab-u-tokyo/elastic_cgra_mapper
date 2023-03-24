#include <simulator/elastic_PE.hpp>

simulator::ElasticPE::ElasticPE(
    int buffer_size, int config_size,
    std::shared_ptr<simulator::Memory> memory_ptr, int row_id, int column_id,
    std::function<int(entity::OpType, int, int)> execute_operation_func)
    : position_id_(row_id, column_id),
      memory_ptr_(memory_ptr),
      buffer_size_(buffer_size),
      config_size_(config_size) {
  simulator::ElasticWire<int> mux_1_to_join, mux_2_to_join, mux_to_fork,
      mux_to_bypass, join_to_VLU_1, join_to_VLU_2, VLU_to_buffer, buffer_to_mux,
      bypass_to_mux, bypass_mux_to_mux;

  // initialize mux
  input_mux_1_ = simulator::ElasticMultiPlexer<int>(mux_1_to_join, config_size);
  input_mux_2_ = simulator::ElasticMultiPlexer<int>(mux_2_to_join, config_size);
  output_mux_ = simulator::ElasticMultiPlexer<int>(mux_to_fork, config_size);
  output_mux_.SetInputWire(bypass_position_, bypass_mux_to_mux);
  output_mux_.SetInputWire(buffer_position_, buffer_to_mux);
  bypass_mux_ =
      simulator::ElasticMultiPlexer<int>(bypass_mux_to_mux, config_size);

  // initialize fork
  output_fork_ = simulator::ElasticFork<int>(config_size);
  output_fork_.SetInputWire(mux_to_fork);

  // initialize join
  join_ = simulator::ElasticJoin<int>();
  join_.SetWire(mux_1_to_join, join_to_VLU_1);
  join_.SetWire(mux_2_to_join, join_to_VLU_2);

  // initialize VLU
  std::vector<simulator::ElasticWire<int>> VLU_input_vec = {join_to_VLU_1,
                                                            join_to_VLU_2};
  VLU_ = simulator::ElasticVLU<int>(VLU_input_vec, VLU_to_buffer, config_size,
                                    execute_operation_func);

  // initialize buffer
  buffer_ =
      simulator::ElasticBuffer<int>(VLU_to_buffer, buffer_to_mux, buffer_size);

  // initialize wire vec
  elastic_wire_vec_ = {mux_1_to_join,    mux_2_to_join, mux_to_fork,
                       mux_to_bypass,    join_to_VLU_1, join_to_VLU_2,
                       VLU_to_buffer,    buffer_to_mux, bypass_to_mux,
                       bypass_mux_to_mux};

  return;
};

void simulator::ElasticPE::SetInputWire(entity::PEPositionId position_id,
                                        ElasticWire<int> wire) {
  simulator::ElasticFork<int> new_elastic_fork(config_size_);
  simulator::ElasticWire<int> fork_to_mux_1, fork_to_mux_2, fork_to_bypass;

  new_elastic_fork.SetOutputWire(position_id, fork_to_mux_1);
  new_elastic_fork.SetOutputWire(position_id, fork_to_mux_2);
  new_elastic_fork.SetOutputWire(position_id, fork_to_bypass);
  new_elastic_fork.SetInputWire(wire);

  elastic_wire_vec_.push_back(fork_to_mux_1);
  elastic_wire_vec_.push_back(fork_to_mux_2);
  elastic_wire_vec_.push_back(fork_to_bypass);
  input_fork_vec_.push_back(new_elastic_fork);

  input_mux_1_.SetInputWire(position_id, wire);
  input_mux_2_.SetInputWire(position_id, wire);
  bypass_mux_.SetInputWire(position_id, wire);

  return;
}

void simulator::ElasticPE::SetOutputWire(entity::PEPositionId position_id,
                                         ElasticWire<int> wire) {
  output_fork_.SetOutputWire(position_id, wire);

  return;
}