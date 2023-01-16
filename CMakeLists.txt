cmake_minimum_required(VERSION 3.4.3)
SET (CMAKE_CXX_FLAGS "-std=c++17")

project(ElasticCGRAMapper)

add_subdirectory(third_party)

add_executable(main src/main.cpp)
set_property(TARGET main PROPERTY CXX_STANDARD 17)
target_link_libraries(main PRIVATE boost_array)


