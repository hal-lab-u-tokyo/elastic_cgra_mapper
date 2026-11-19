#include <Vsynchronous_CGRA.h>
#include <gtest/gtest.h>
#include <verilated.h>

#include <io/architecture_io.hpp>
#include <io/dfg_io.hpp>
#include <io/mapping_io.hpp>
#include <iostream>
#include <vector>
#include <verilog_runner/simulation_runner.hpp>

TEST(VerilogSimulatorTest, elastic_CGRA_test) {
  std::string mapping_file_path =
      std::string(VERILOG_TEST_DATA_DIR) + "/elastic_mapping.json";
  std::string fst_output_file_path =
      std::string(VERILOG_TEST_DATA_DIR) + "/elastic_CGRA_test.fst";

  simulator::SimulationRunner simulation_runner(mapping_file_path,
                                                fst_output_file_path);
  simulation_runner.RunSimulation(200);
  simulation_runner.VerifySimulationResult();

  std::cout << "Simulation completed. Verifying results..." << std::endl;
  auto simulation_result_map = simulation_runner.GetSimulationResult();
  auto expected_result_map = simulation_runner.GetExpectedResult();
  std::cout << "Expected result map size: " << expected_result_map.size()
            << std::endl;
  std::cout << "Simulation result map size: " << simulation_result_map.size()
            << std::endl;

  EXPECT_EQ(simulation_result_map.size(), expected_result_map.size());
  for (const auto& [config_id, expected_output] : expected_result_map) {
    auto it = simulation_result_map.find(config_id);
    EXPECT_NE(it, simulation_result_map.end());
    if (it != simulation_result_map.end()) {
      std::vector<int> actual_output = it->second;
      EXPECT_EQ(actual_output, expected_output);
    }
  }
}
