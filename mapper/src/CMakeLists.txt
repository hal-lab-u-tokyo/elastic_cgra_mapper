add_library(mapper_common INTERFACE)
target_compile_features(mapper_common INTERFACE cxx_std_17)
target_include_directories(mapper_common
                           INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/../include)
target_link_libraries(mapper_common INTERFACE entity gurobi::cxx stdc++fs)
target_compile_options(mapper_common INTERFACE -Wall -Wextra -Wno-deprecated)
target_sources(mapper_common
               INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/gurobi_placement_mapper.cpp)

add_library(mapper_new INTERFACE)
target_link_libraries(mapper_new INTERFACE mapper_common)
target_sources(mapper_new
               INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/gurobi_mapper.cpp)

add_library(mapper_agilex INTERFACE)
target_link_libraries(mapper_agilex INTERFACE mapper_common)
target_sources(mapper_agilex
               INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/gurobi_mapper_AgileX.cpp)

add_library(mapper INTERFACE)
target_link_libraries(mapper INTERFACE mapper_new)
