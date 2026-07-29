#pragma once

#include <entity/mapping.hpp>
#include <filesystem>
#include <verilog_runner/cgra_wrapper.hpp>

namespace simulator {

class SimulationRunner {
 public:
  SimulationRunner(const std::string& mapping_file_path,
                   const std::string& fst_output_file_path);

  void VerifySimulationResult();
  void RunSimulation(int max_cycle = 100);
  SimulationResultMap GetSimulationResult() const {
    if (simulation_result_map_.empty()) {
      std::cerr << "Simulation result is empty. Please run the simulation and "
                   "verify the result."
                << std::endl;
    }
    return simulation_result_map_;
  }
  SimulationResultMap GetExpectedResult() const {
    if (expected_result_map_.empty()) {
      std::cerr << "Expected result is empty. Please run the simulation and "
                   "verify the result."
                << std::endl;
    }
    return expected_result_map_;
  }

 private:
  SimulationResultMap CalculateExpectedOutput(int max_cycle);
  SimulationResultMap CreateSimulationResultMap();

  void WriteConfigToCGRA();
  void WriteMemoryDataToCGRA();
  void SetParameters();
  int ExecuteOperation(entity::OpType op_type, int input1, int input2,
                       int const_value);
  void InitializeMemoryData();

  entity::Mapping mapping_result_;
  std::vector<int> memory_data_;
  std::unique_ptr<simulator::CGRAWrapper> cgra_;
  std::filesystem::path fst_output_file_path_;
  std::unordered_map<entity::ConfigId, int, entity::HashConfigId>
      dummy_output_num_map_;
  SimulationResultMap simulation_result_map_;
  SimulationResultMap expected_result_map_;

  int simulation_exec_cycle_ = 0;
};
}  // namespace simulator
