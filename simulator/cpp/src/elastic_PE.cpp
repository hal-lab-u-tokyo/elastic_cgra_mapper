#include <simulator/elastic_PE.hpp>

const entity::PEPositionId kBypassPosition = entity::PEPositionId(-1, -1);
const entity::PEPositionId kBufferPosition = entity::PEPositionId(-2, -2);

simulator::ElasticPE::ElasticPE(
    int buffer_size, int config_size,
    std::shared_ptr<simulator::Memory> memory_ptr, int row_id, int column_id,
    std::function<int(entity::OpType, int, int,
                      std::shared_ptr<simulator::Memory>, entity::CGRAConfig)>
        execute_operation_func)
    : position_id_(row_id, column_id),
      memory_ptr_(memory_ptr),
      buffer_size_(buffer_size),
      config_size_(config_size) {
  simulator::ElasticWire<int> mux_1_to_join, mux_2_to_join, mux_to_fork,
      mux_to_bypass, join_to_VLU_1, join_to_VLU_2, VLU_to_buffer, buffer_to_reg,
      reg_to_mux, bypass_to_mux, bypass_mux_to_mux;

  // initialize mux
  input_mux_1_ = simulator::ElasticMultiPlexer<int>(mux_1_to_join, config_size);
  input_mux_2_ = simulator::ElasticMultiPlexer<int>(mux_2_to_join, config_size);
  output_mux_ = simulator::ElasticMultiPlexer<int>(mux_to_fork, config_size);
  output_mux_.SetInputWire(kBypassPosition, bypass_mux_to_mux);
  output_mux_.SetInputWire(kBufferPosition, reg_to_mux);
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
                                    execute_operation_func, memory_ptr_);

  // initialize buffer
  buffer_ =
      simulator::ElasticBuffer<int>(VLU_to_buffer, buffer_to_reg, buffer_size);

  // initialize register
  register_ = simulator::ElasticRegister<int>(buffer_to_reg, reg_to_mux);

  return;
};

void simulator::ElasticPE::SetConfig(int id, entity::CGRAConfig config) {
  if (config.from_config_id_num >= 1) {
    input_mux_1_.SetConfig(id, config.from_config_id_vec[0].GetPositionId());
  }
  if (config.from_config_id_num == 2) {
    input_mux_2_.SetConfig(id, config.from_config_id_vec[1].GetPositionId());
  }

  if (config.operation_type == entity::OpType::ROUTE) {
    output_mux_.SetConfig(id, kBypassPosition);
    bypass_mux_.SetConfig(id, config.from_config_id_vec[0].GetPositionId());
  } else {
    output_mux_.SetConfig(id, kBufferPosition);
  }

  std::vector<entity::PEPositionId> output_position_id_vec;
  for (auto config_id : config.to_config_id_vec) {
    output_position_id_vec.push_back(config_id.GetPositionId());
  }
  output_fork_.SetConfig(id, output_position_id_vec);

  VLU_.SetConfig(id, config);
}

void simulator::ElasticPE::SetInputWire(entity::PEPositionId position_id,
                                        ElasticWire<int> wire) {
  simulator::ElasticFork<int> new_elastic_fork(config_size_);
  simulator::ElasticWire<int> fork_to_mux_1, fork_to_mux_2, fork_to_bypass;

  new_elastic_fork.SetOutputWire(position_id, fork_to_mux_1);
  new_elastic_fork.SetOutputWire(position_id, fork_to_mux_2);
  new_elastic_fork.SetOutputWire(position_id, fork_to_bypass);
  new_elastic_fork.SetInputWire(wire);

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

void simulator::ElasticPE::Update() { return; };

void simulator::ElasticPE::RegisterUpdate() {
  register_.Update();
  return;
}
