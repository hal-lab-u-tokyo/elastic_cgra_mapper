#include <verilog_runner/cgra_wrapper.hpp>

int GetInputPEIndex(entity::ConfigId from, entity::ConfigId to) {
  if (from.row_id < to.row_id) return 0;
  if (from.row_id > to.row_id) return 1;
  if (from.column_id < to.column_id) return 2;
  if (from.column_id > to.column_id) return 3;
  return 4;
}

int GetOpIndex(entity::OpType op) {
  if (op == entity::OpType::kNop) return 0;
  if (op == entity::OpType::kAdd) return 1;
  if (op == entity::OpType::kSub) return 2;
  if (op == entity::OpType::kMul) return 3;
  if (op == entity::OpType::kDiv) return 4;
  if (op == entity::OpType::kConst) return 5;
  if (op == entity::OpType::kLoad) return 6;
  if (op == entity::OpType::kOutput) return 7;
  if (op == entity::OpType::kRoute) return 8;
  return 0;
}

simulator::CGRAWrapper::CGRAWrapper(const VerilatedVcdC* tfp,
                                    entity::MRRGConfig mrrg_config)
    : tfp_(const_cast<VerilatedVcdC*>(tfp)), mrrg_config_(mrrg_config) {
  cgra_ = new Vsynchronous_CGRA();
  cgra_->trace(tfp_, 100);
}

void simulator::CGRAWrapper::WriteConfigToCGRA(
    const entity::ConfigId config_id, const entity::CGRAConfig& config) {
  CGRAInputData input_data;
  input_data.SetData(config_id.row_id, CGRAPortName::kConfigPERowIndex);
  input_data.SetData(config_id.column_id, CGRAPortName::kConfigPEColumnIndex);

  const int input_pe_index_1 =
      (config.from_config_id_vec.size() > 0)
          ? GetInputPEIndex(config.from_config_id_vec[0], config_id)
          : 4;
  const int input_pe_index_2 =
      (config.from_config_id_vec.size() > 1)
          ? GetInputPEIndex(config.from_config_id_vec[1], config_id)
          : 4;
  input_data.SetData(input_pe_index_1, CGRAPortName::kConfigInputPEIndex1);
  input_data.SetData(input_pe_index_2, CGRAPortName::kConfigInputPEIndex2);

  input_data.SetData(GetOpIndex(config.operation_type),
                     CGRAPortName::kConfigOp);
  input_data.SetData(config.const_value, CGRAPortName::kConfigConstData);
  input_data.SetData(1, CGRAPortName::kWriteConfigData);
  input_data.SetData(config_id.context_id, CGRAPortName::kConfigIndex);

  SetPortData(input_data);
  UpdateCycle(1);
}

void simulator::CGRAWrapper::WriteMemoryDataToCGRA(int data, int address) {
  CGRAInputData input_data;
  input_data.SetData(address, CGRAPortName::kMemoryWriteAddress);
  input_data.SetData(data, CGRAPortName::kMemoryWriteData);
  input_data.SetData(1, CGRAPortName::kMemoryWrite);

  SetPortData(input_data);
  UpdateCycle(1);
}

void simulator::CGRAWrapper::SetPortData(CGRAInputData data) {
  cgra_->clk = data.GetData(CGRAPortName::kClk);
  cgra_->reset_n = data.GetData(CGRAPortName::kResetN);
  cgra_->config_PE_row_index = data.GetData(CGRAPortName::kConfigPERowIndex);
  cgra_->config_PE_column_index =
      data.GetData(CGRAPortName::kConfigPEColumnIndex);
  cgra_->config_input_PE_index_1 =
      data.GetData(CGRAPortName::kConfigInputPEIndex1);
  cgra_->config_input_PE_index_2 =
      data.GetData(CGRAPortName::kConfigInputPEIndex2);
  cgra_->config_op = data.GetData(CGRAPortName::kConfigOp);
  cgra_->config_const_data = data.GetData(CGRAPortName::kConfigConstData);
  cgra_->write_config_data = data.GetData(CGRAPortName::kWriteConfigData);
  cgra_->config_index = data.GetData(CGRAPortName::kConfigIndex);
  cgra_->start_exec = data.GetData(CGRAPortName::kStartExec);
  cgra_->mapping_context_max_id =
      data.GetData(CGRAPortName::kMappingContextMaxId);
  cgra_->memory_write_address = data.GetData(CGRAPortName::kMemoryWriteAddress);
  cgra_->memory_write = data.GetData(CGRAPortName::kMemoryWrite);
  cgra_->memory_write_data = data.GetData(CGRAPortName::kMemoryWriteData);
}

void simulator::CGRAWrapper::Execute(int cycles) {
  CGRAInputData input_data;
  input_data.SetData(1, CGRAPortName::kStartExec);
  input_data.SetData(mrrg_config_.context_size - 1,
                     CGRAPortName::kMappingContextMaxId);

  SetPortData(input_data);
  UpdateCycle(1);
  input_data.SetData(0, CGRAPortName::kStartExec);
  SetPortData(input_data);
  UpdateCycle(cycles - 1);
}

void simulator::CGRAWrapper::Initialize() {
  CGRAInputData input_data;
  input_data.SetData(0, CGRAPortName::kResetN);
  SetPortData(input_data);
  UpdateCycle(10);
}

void simulator::CGRAWrapper::UpdateCycle(int cycle) {
  for (int i = 0; i < cycle * 2 * kClockHalfPeriod; i++) {
    if ((time_ % kClockHalfPeriod) == 0) {
      cgra_->clk = !cgra_->clk;  // Toggle clock
    }
    cgra_->eval();
    if ((time_ % kClockHalfPeriod) == 0 && cgra_->clk) {
      SaveOutputData();
    }
    tfp_->dump(time_);
    time_++;
  }
}

void simulator::CGRAWrapper::SaveOutputData() {
  for (int i = 0; i < mrrg_config_.row; i++) {
    for (int j = 0; j < mrrg_config_.column; j++) {
      if (cgra_->DEBUG_valid_output[i][j]) {
        int context_id = cgra_->DEBUG_output_context_id[i][j];
        output_data_[entity::ConfigId(i, j, context_id)].push_back(
            cgra_->pe_output[i][j]);
      }
    }
  }
}
