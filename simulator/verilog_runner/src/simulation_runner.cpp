#include <io/mapping_io.hpp>
#include <queue>
#include <verilog_runner/simulation_runner.hpp>

simulator::SimulationRunner::SimulationRunner(
    const std::string& mapping_file_path,
    const std::string& fst_output_file_path) {
  mapping_result_ = io::ReadMappingFile(mapping_file_path);
  fst_output_file_path_ = std::filesystem::path(fst_output_file_path);
  VerilatedVcdC* tfp = new VerilatedVcdC;
  cgra_ = std::make_unique<simulator::CGRAWrapper>(
      tfp, mapping_result_.GetMRRGConfig());
  std::cout << "Start simulation..." << std::endl;
  SetParameters();
  InitializeMemoryData();
}

void simulator::SimulationRunner::VerifySimulationResult() {
  std::cout << "Verifying simulation result..." << std::endl;
  expected_result_map_ = CalculateExpectedOutput(simulation_exec_cycle_);
  simulation_result_map_ = CreateSimulationResultMap();

  for (const auto& id_and_output : expected_result_map_) {
    const entity::ConfigId& config_id = id_and_output.first;
    const std::vector<int>& expected_values = id_and_output.second;
    const std::vector<int>& actual_values =
        simulation_result_map_.at(config_id);

    if (expected_values.size() != actual_values.size()) {
      std::cerr << "Mismatch in output size for ConfigId (row: "
                << config_id.row_id << ", column: " << config_id.column_id
                << ", context: " << config_id.context_id << "): expected "
                << expected_values.size() << ", got " << actual_values.size()
                << std::endl;
      continue;
    }

    for (size_t i = 0; i < expected_values.size(); i++) {
      if (expected_values[i] != actual_values[i]) {
        std::cerr << "Mismatch at ConfigId (row: " << config_id.row_id
                  << ", column: " << config_id.column_id
                  << ", context: " << config_id.context_id << "), index " << i
                  << ": expected " << expected_values[i] << ", got "
                  << actual_values[i] << std::endl;
      }
    }

    // output short summary (output size and the output) for each
    // ConfigId
    std::cout << "ConfigId (row: " << config_id.row_id
              << ", column: " << config_id.column_id
              << ", context: " << config_id.context_id
              << ") actual: " << actual_values.size()
              << ", expected: " << expected_values.size() << std::endl;

    // lambda function to print the output values
    auto print_output_values = [](const std::vector<int>& values) {
      std::cout << "[";
      for (size_t i = 0; i < values.size(); i++) {
        std::cout << values[i];
        if (i != values.size() - 1) {
          std::cout << ", ";
        }
      }
      std::cout << "]" << std::endl;
    };

    std::cout << "Actual output values: ";
    print_output_values(actual_values);
    std::cout << "Expected output values: ";
    print_output_values(expected_values);
  }
}

void simulator::SimulationRunner::RunSimulation(int max_cycle) {
  WriteConfigToCGRA();
  WriteMemoryDataToCGRA();
  cgra_->Execute(max_cycle);
  simulation_exec_cycle_ = max_cycle;
}

simulator::SimulationResultMap
simulator::SimulationRunner::CalculateExpectedOutput(int max_cycle) {
  entity::DFG dfg = mapping_result_.GenerateDFGFromMapping();
  std::vector<entity::ConfigId> output_config_id_vec;
  std::vector<int> output_dfg_id_vec;
  std::unordered_map<entity::ConfigId, int, entity::HashConfigId>
      config_id_to_dfg_id_map;

  for (const auto& id_and_config : mapping_result_.GetConfigMap()) {
    if (id_and_config.second.operation_type == entity::OpType::kOutput) {
      output_config_id_vec.push_back(id_and_config.first);
    }
  }

  for (int i = 0; i < dfg.GetNodeNum(); i++) {
    if (dfg.GetNodeProperty(i).op == entity::OpType::kOutput) {
      output_dfg_id_vec.push_back(i);
    }
  }

  for (const auto& id : output_config_id_vec) {
    for (int dfg_id : output_dfg_id_vec) {
      if (dfg.GetNodeProperty(dfg_id).op_name ==
          mapping_result_.GetConfig(id).operation_name) {
        config_id_to_dfg_id_map.emplace(id, dfg_id);
        break;
      }
    }
  }

  SimulationResultMap expected_output;
  for (const auto& id : output_config_id_vec) {
    int dfg_id = config_id_to_dfg_id_map.at(id);
    int num_of_output =
        (max_cycle - 1 - mapping_result_.GetCriticalPathLength(id)) /
            mapping_result_.GetMRRGConfig().context_size +
        1;
    std::vector<int> output_values =
        dfg.Execute(memory_data_, num_of_output, dfg_id);
    expected_output.emplace(id, output_values);
  }

  return expected_output;
}

simulator::SimulationResultMap
simulator::SimulationRunner::CreateSimulationResultMap() {
  SimulationResultMap simulation_result_map_;
  for (const auto& config_id_to_dummy_output_num : dummy_output_num_map_) {
    const entity::ConfigId& config_id = config_id_to_dummy_output_num.first;
    int dummy_output_num = config_id_to_dummy_output_num.second;
    std::vector<int> actual_values = cgra_->GetOutputData(config_id);
    std::vector<int> output_values(actual_values.begin() + dummy_output_num,
                                   actual_values.end());
    simulation_result_map_.emplace(config_id, output_values);
  }
  return simulation_result_map_;
}

void simulator::SimulationRunner::WriteConfigToCGRA() {
  for (const auto& id_and_config : mapping_result_.GetConfigMap()) {
    cgra_->WriteConfigToCGRA(id_and_config.first, id_and_config.second);
  }
}

void simulator::SimulationRunner::WriteMemoryDataToCGRA() {
  for (size_t i = 0; i < cgra_->GetDataSize(); i++) {
    cgra_->WriteMemoryDataToCGRA(memory_data_[i], i);
  }
}

void simulator::SimulationRunner::SetParameters() {
  if (mapping_result_.GetMRRGConfig().cgra_type ==
      entity::MRRGCGRAType::kElastic) {
    return;
  }

  for (const auto& id_and_config : mapping_result_.GetConfigMap()) {
    if (id_and_config.second.operation_type == entity::OpType::kOutput) {
      int output_critical_path_length =
          mapping_result_.GetCriticalPathLength(id_and_config.first);
      std::cout << "ConfigId (row: " << id_and_config.first.row_id
                << ", column: " << id_and_config.first.column_id
                << ", context: " << id_and_config.first.context_id
                << ") critical path length: " << output_critical_path_length
                << std::endl;
      dummy_output_num_map_.emplace(
          id_and_config.first,
          output_critical_path_length /
              mapping_result_.GetMRRGConfig().context_size);
    }
  }
}

void simulator::SimulationRunner::InitializeMemoryData() {
  memory_data_.resize(cgra_->GetDataSize());
  for (size_t i = 0; i < cgra_->GetDataSize(); i++) {
    memory_data_[i] = i;  // Initialize with some values, e.g., 0-255 repeating
  }
}
