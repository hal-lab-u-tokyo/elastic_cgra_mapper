add_library(cpp_simulator INTERFACE IMPORTED GLOBAL)
set_property(TARGET cpp_simulator PROPERTY CXX_STANDARD 17)
set_target_properties(
  cpp_simulator PROPERTIES INTERFACE_INCLUDE_DIRECTORIES
                       ${CMAKE_CURRENT_SOURCE_DIR}/../include)

target_link_libraries(cpp_simulator INTERFACE entity)
target_compile_options(cpp_simulator INTERFACE -Wall -Wextra -Wno-deprecated)
target_sources(cpp_simulator INTERFACE CGRA.cpp memory.cpp PE.cpp elastic_PE.cpp
                                   elastic_CGRA.cpp)
