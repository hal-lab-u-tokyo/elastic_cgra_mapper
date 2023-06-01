add_library(simulator INTERFACE IMPORTED GLOBAL)
set_property(TARGET simulator PROPERTY CXX_STANDARD 17)
set_target_properties(
  simulator PROPERTIES INTERFACE_INCLUDE_DIRECTORIES
                       ${CMAKE_CURRENT_SOURCE_DIR}/../include)

target_link_libraries(simulator INTERFACE entity)
target_compile_options(simulator INTERFACE -Wall -Wextra -Wno-deprecated)
target_sources(simulator INTERFACE CGRA.cpp memory.cpp PE.cpp elastic_PE.cpp
                                   elastic_CGRA.cpp)
