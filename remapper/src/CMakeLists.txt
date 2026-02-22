add_library(remapper INTERFACE IMPORTED GLOBAL)
set_property(TARGET remapper PROPERTY CXX_STANDARD 17)
set_target_properties(
  remapper PROPERTIES INTERFACE_INCLUDE_DIRECTORIES
                      ${CMAKE_CURRENT_SOURCE_DIR}/../include)

target_link_libraries(remapper INTERFACE entity gurobi::cxx stdc++fs
                                         Eigen3::Eigen)
target_compile_options(remapper INTERFACE -Wall -Wextra -Wno-deprecated)
target_sources(
  remapper
  INTERFACE remapper.cpp
            mapping_concater.cpp
            algorithm_entity.cpp
            combination_counter.cpp
            transform.cpp
            algorithm/full_search_remapper.cpp
            algorithm/greedy_elastic_remapper.cpp
            algorithm/dp_elastic_remapper.cpp
            algorithm/dp_and_full_search_elastic_remapper.cpp
            mapping_transform_op.cpp)
