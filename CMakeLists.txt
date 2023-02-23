cmake_minimum_required(VERSION 3.20)
set(CMAKE_CXX_FLAGS "-std=c++17")

project(ElasticCGRAMapper)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
set(PROJECT_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(ENV{GUROBI_HOME} /opt/gurobi911/linux64)

find_package(Gurobi REQUIRED)

add_subdirectory(third_party)

enable_testing()

add_subdirectory(entity)
add_subdirectory(io)
add_subdirectory(mapper)

add_executable(main src/main.cpp)
set_property(TARGET main PROPERTY CXX_STANDARD 17)
target_link_libraries(main PRIVATE Boost::array Boost::graph gurobi::cxx io entity)
