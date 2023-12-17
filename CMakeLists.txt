cmake_minimum_required(VERSION 3.20)
set(CMAKE_CXX_FLAGS "-std=c++17")
set(CMAKE_CXX_FLAGS_DEBUG "-g")

project(ElasticCGRAMapper)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
set(PROJECT_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR})

find_package(Gurobi REQUIRED)
find_package(verilator HINTS $ENV{VERILATOR_ROOT})

set(CMAKE_EXPORT_COMPILE_COMMANDS on)

add_subdirectory(third_party)

enable_testing()

add_subdirectory(entity)
add_subdirectory(io)
add_subdirectory(mapper)
add_subdirectory(simulator)
add_subdirectory(remapper)

add_executable(mapping src/mapping.cpp)
set_property(TARGET mapping PROPERTY CXX_STANDARD 17)
target_compile_options(mapping INTERFACE -Wall -Wextra -Wno-deprecated)
target_link_libraries(mapping PRIVATE io entity mapper cpp_simulator)

add_executable(create_database src/create_database.cpp)
set_property(TARGET create_database PROPERTY CXX_STANDARD 17)
target_compile_options(create_database INTERFACE -Wall -Wextra -Wno-deprecated)
target_link_libraries(create_database PRIVATE io entity mapper cpp_simulator)

add_executable(bulk_mapping src/bulk_mapping.cpp)
set_property(TARGET bulk_mapping PROPERTY CXX_STANDARD 17)
target_compile_options(bulk_mapping INTERFACE -Wall -Wextra -Wno-deprecated)
target_link_libraries(bulk_mapping PRIVATE io entity mapper cpp_simulator)

add_executable(verilator_synchronous_CGRA src/verilator_synchronous_CGRA.cpp)
verilate(verilator_synchronous_CGRA TRACE INCLUDE_DIRS
         ${CMAKE_CURRENT_SOURCE_DIR}/simulator/verilog/src/ SOURCES
         ${CMAKE_CURRENT_SOURCE_DIR}/simulator/verilog/src/synchronous_CGRA.v)
set_property(TARGET verilator_synchronous_CGRA PROPERTY CXX_STANDARD 17)
target_compile_options(verilator_synchronous_CGRA INTERFACE -Wall -Wextra
                                                            -Wno-deprecated)
target_link_libraries(verilator_synchronous_CGRA PRIVATE io entity mapper
                                                         cpp_simulator)
add_executable(remapping src/elastic_remapping.cpp)
set_property(TARGET remapping PROPERTY CXX_STANDARD 17)
target_compile_options(remapping INTERFACE -Wall -Wextra -Wno-deprecated)
target_link_libraries(remapping PRIVATE io entity remapper)
