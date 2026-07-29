#pragma once
#include <Vsynchronous_CGRA.h>
#include <verilated.h>
#include <verilated_vcd_c.h>

#include <entity/mapping.hpp>

namespace simulator {
typedef std::unordered_map<entity::ConfigId, std::vector<int>,
                           entity::HashConfigId>
    SimulationResultMap;

enum class CGRAPortName {
  kClk,
  kResetN,
  kConfigPERowIndex,
  kConfigPEColumnIndex,
  kConfigInputPEIndex1,
  kConfigInputPEIndex2,
  kConfigOp,
  kConfigConstData,
  kWriteConfigData,
  kConfigIndex,
  kStartExec,
  kMappingContextMaxId,
  kMemoryWriteAddress,
  kMemoryWrite,
  kMemoryWriteData,
  kMemoryReadAddress,
};

struct CGRAInputData {
  CGRAInputData() {
    cgra_input.resize(16, 0);
    SetData(1, CGRAPortName::kResetN);
  };
  void SetData(int data, CGRAPortName port_name) {
    cgra_input[static_cast<int>(port_name)] = data;
  }
  int GetData(CGRAPortName port_name) const {
    return cgra_input[static_cast<int>(port_name)];
  }
  std::vector<int> cgra_input;
};

class CGRAWrapper {
 public:
  CGRAWrapper(const VerilatedVcdC* tfp, entity::MRRGConfig mrrg_config);
  void WriteConfigToCGRA(const entity::ConfigId config_id,
                         const entity::CGRAConfig& config);
  void WriteMemoryDataToCGRA(int data, int address);
  void SetPortData(CGRAInputData data);
  void Execute(int cycles);
  void Initialize();
  void UpdateCycle(int cycle);
  std::vector<int> GetOutputData(const entity::ConfigId& config_id) const {
    if (output_data_.count(config_id) == 0) {
      return {};
    }
    return output_data_.at(config_id);
  }
  int GetDataSize() const { return data_size_; }

 private:
  void SaveOutputData();
  Vsynchronous_CGRA* cgra_;
  int time_ = 0;
  int data_size_ = 1 << 16;  // 64KB
  static constexpr int kClockHalfPeriod = 1;
  VerilatedVcdC* tfp_;
  SimulationResultMap output_data_;
  entity::MRRGConfig mrrg_config_;
};
}  // namespace simulator
