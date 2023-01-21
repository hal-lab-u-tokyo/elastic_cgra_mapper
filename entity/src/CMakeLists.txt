add_library(entity INTERFACE IMPORTED GLOBAL)
set_property(TARGET entity PROPERTY CXX_STANDARD 17)
set_target_properties(entity PROPERTIES INTERFACE_INCLUDE_DIRECTORIES
                                        ${CMAKE_CURRENT_SOURCE_DIR}/../include)

target_link_libraries(entity INTERFACE Boost::graph)

target_sources(entity INTERFACE dfg.cpp mrrg.cpp graph.cpp)
