add_executable(VerilogDataMemoryTest verilog_data_memory_test.cpp)
verilate(VerilogDataMemoryTest INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/../src/
         SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../src/data_memory.v)
target_link_libraries(VerilogDataMemoryTest PRIVATE gtest_main)

add_test(NAME VerilogDataMemoryTest COMMAND VerilogDataMemoryTest)

add_executable(VerilogSynchronousPETest verilog_synchronous_PE_test.cpp)
verilate(VerilogSynchronousPETest INCLUDE_DIRS
${CMAKE_CURRENT_SOURCE_DIR}/../src/ SOURCES
${CMAKE_CURRENT_SOURCE_DIR}/../src/synchronous_PE.v)
target_link_libraries(VerilogSynchronousPETest PRIVATE gtest_main)

add_test(NAME VerilogSynchronousPETest COMMAND VerilogSynchronousPETest)
