#include <common/logging/debug_logger.hpp>
#include <cxxopts.hpp>
#include <io/architecture_io.hpp>
#include <io/dfg_io.hpp>
#include <io/mapper_config_io.hpp>
#include <io/mapping_io.hpp>
#include <iostream>
#include <mapper/gurobi_placement_mapper.hpp>
#include <verilog_runner/simulation_runner.hpp>

int main(int argc, char** argv) {
  cxxopts::Options options("verilator_synchronous_CGRA",
                           "Run Verilator synchronous CGRA simulation.");
  options.add_options()("mapping_file",
                        "Absolute path to write the mapping json file",
                        cxxopts::value<std::string>())(
      "fst_output_file", "Path to write the waveform output file",
      cxxopts::value<std::string>())(
      "log_output", "Print debug log messages to stdout")("h,help",
                                                          "Print usage");

  std::string mapping_file_path;
  std::string fst_output_file_path;
  bool log_output = false;
  common::logging::DebugLogger logger(log_output);
  logger << "Loading mapping file from: " << mapping_file_path << std::endl;
  try {
    const auto result = options.parse(argc, argv);
    if (result.count("help")) {
      std::cout << options.help();
      return 0;
    }
    mapping_file_path = result["mapping_file"].as<std::string>();
    fst_output_file_path = result["fst_output_file"].as<std::string>();
    log_output = result.count("log_output") > 0;
  } catch (const cxxopts::exceptions::exception& e) {
    std::cerr << "invalid arguments: " << e.what() << std::endl;
    std::cerr << options.help();
    return 1;
  }

  logger << "Executing Verilator synchronous CGRA simulation ..." << std::endl;
  simulator::SimulationRunner simulation_runner(mapping_file_path,
                                                fst_output_file_path);
  simulation_runner.RunSimulation();
  simulation_runner.VerifySimulationResult();
}
