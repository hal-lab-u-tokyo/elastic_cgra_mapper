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

add_executable(VerilogSynchronousCGRATest verilog_synchronous_CGRA_test.cpp)
verilate(VerilogSynchronousCGRATest INCLUDE_DIRS
         ${CMAKE_CURRENT_SOURCE_DIR}/../src/ SOURCES
         ${CMAKE_CURRENT_SOURCE_DIR}/../src/synchronous_CGRA.v)
target_link_libraries(VerilogSynchronousCGRATest PRIVATE gtest_main io mapper)
add_test(NAME VerilogSynchronousCGRATest COMMAND VerilogSynchronousCGRATest)

add_executable(VerilogElasticBufferTest verilog_elastic_buffer_test.cpp)
verilate(VerilogElasticBufferTest INCLUDE_DIRS
         ${CMAKE_CURRENT_SOURCE_DIR}/../src/ SOURCES
         ${CMAKE_CURRENT_SOURCE_DIR}/../src/elastic_module/elastic_buffer.v)
target_link_libraries(VerilogElasticBufferTest PRIVATE gtest_main io mapper)
add_test(NAME VerilogElasticBufferTest COMMAND VerilogElasticBufferTest)

add_executable(VerilogElasticForkTest verilog_elastic_fork_test.cpp)
verilate(VerilogElasticForkTest INCLUDE_DIRS
         ${CMAKE_CURRENT_SOURCE_DIR}/../src/ SOURCES
         ${CMAKE_CURRENT_SOURCE_DIR}/../src/elastic_module/elastic_fork.v)
target_link_libraries(VerilogElasticForkTest PRIVATE gtest_main io mapper)
add_test(NAME VerilogElasticForkTest COMMAND VerilogElasticForkTest)

add_executable(VerilogElasticJoinTest verilog_elastic_join_test.cpp)
verilate(VerilogElasticJoinTest INCLUDE_DIRS
         ${CMAKE_CURRENT_SOURCE_DIR}/../src/ SOURCES
         ${CMAKE_CURRENT_SOURCE_DIR}/../src/elastic_module/elastic_join.v)
target_link_libraries(VerilogElasticJoinTest PRIVATE gtest_main io mapper)
add_test(NAME VerilogElasticJoinTest COMMAND VerilogElasticJoinTest)

add_executable(VerilogElasticMultiplexerTest
               verilog_elastic_multiplexer_test.cpp)
verilate(
  VerilogElasticMultiplexerTest INCLUDE_DIRS
  ${CMAKE_CURRENT_SOURCE_DIR}/../src/ SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/../src/elastic_module/elastic_multiplexer.v)
target_link_libraries(VerilogElasticMultiplexerTest PRIVATE gtest_main io
                                                            mapper)
add_test(NAME VerilogElasticMultiplexerTest
         COMMAND VerilogElasticMultiplexerTest)

add_executable(VerilogElasticALUTest verilog_elastic_alu_test.cpp)
verilate(VerilogElasticALUTest INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/../src/
         SOURCES
         ${CMAKE_CURRENT_SOURCE_DIR}/../src/elastic_module/elastic_alu.v)
target_link_libraries(VerilogElasticALUTest PRIVATE gtest_main io mapper)
add_test(NAME VerilogElasticALUTest COMMAND VerilogElasticALUTest)

add_executable(VerilogElasticPETest verilog_elastic_PE_test.cpp)
verilate(VerilogElasticPETest INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/../src/
         SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../src/elastic_PE.v)
target_link_libraries(VerilogElasticPETest PRIVATE gtest_main io mapper)
add_test(NAME VerilogElasticPETest COMMAND VerilogElasticPETest)
