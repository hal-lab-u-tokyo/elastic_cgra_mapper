add_executable(VerilogSimulatorTest verilog_simulator_test.cpp)
verilate(
  VerilogSimulatorTest
  INCLUDE_DIRS
  ${CMAKE_CURRENT_SOURCE_DIR}/../src/
  SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/../src/data_memory.v)
target_link_libraries(VerilogSimulatorTest PRIVATE gtest_main)

add_test(NAME VerilogSimulatorTest COMMAND VerilogSimulatorTest)
