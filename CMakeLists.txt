cmake_minimum_required(VERSION 3.20)
set(CMAKE_CXX_FLAGS "-std=c++17")

project(ElasticCGRAMapper)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
set(PROJECT_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(ENV{GUROBI_HOME} /opt/gurobi911/linux64)

find_package(Gurobi REQUIRED)
find_package(verilator HINTS $ENV{VERILATOR_ROOT})

add_subdirectory(third_party)

enable_testing()

add_subdirectory(entity)
add_subdirectory(io)
add_subdirectory(mapper)
add_subdirectory(simulator)

add_executable(main example/main.cpp)
set_property(TARGET main PROPERTY CXX_STANDARD 17)
target_compile_options(main INTERFACE -Wall -Wextra -Wno-deprecated)
target_link_libraries(main PRIVATE io entity mapper cpp_simulator)

add_executable(verilator_test example/verilator_test.cpp)
verilate(verilator_test TRACE INCLUDE_DIRS
         ${CMAKE_CURRENT_SOURCE_DIR}/simulator/verilog/src/ SOURCES
         ${CMAKE_CURRENT_SOURCE_DIR}/simulator/verilog/src/synchronous_CGRA.v)
set_property(TARGET verilator_test PROPERTY CXX_STANDARD 17)
target_compile_options(verilator_test INTERFACE -Wall -Wextra -Wno-deprecated)
target_link_libraries(verilator_test PRIVATE io entity mapper cpp_simulator)
