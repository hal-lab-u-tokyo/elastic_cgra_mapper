add_library(verilog_runner STATIC simulation_runner.cpp cgra_wrapper.cpp)
set_property(TARGET verilog_runner PROPERTY CXX_STANDARD 17)
target_include_directories(verilog_runner
                           PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../include)

target_link_libraries(verilog_runner PRIVATE entity io)
verilate(verilog_runner TRACE INCLUDE_DIRS
         ${CMAKE_CURRENT_SOURCE_DIR}/../../verilog/src/ SOURCES
         ${CMAKE_CURRENT_SOURCE_DIR}/../../verilog/src/synchronous_CGRA.v)
target_compile_options(verilog_runner PRIVATE -Wall -Wextra -Wno-deprecated)
