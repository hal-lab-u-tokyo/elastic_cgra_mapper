add_library(mapper INTERFACE IMPORTED GLOBAL)
set_property(TARGET mapper PROPERTY CXX_STANDARD 17)
set_target_properties(mapper PROPERTIES INTERFACE_INCLUDE_DIRECTORIES
                                        ${CMAKE_CURRENT_SOURCE_DIR}/../include)
                            
target_link_libraries(mapper INTERFACE entity gurobi::cxx)
target_compile_options(mapper INTERFACE -Wall -Wextra -Wno-deprecated)
target_sources(mapper INTERFACE gurobi_mapper.cpp)
