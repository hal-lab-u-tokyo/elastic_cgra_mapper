add_library(io INTERFACE IMPORTED GLOBAL)
set_property(TARGET io PROPERTY CXX_STANDARD 17)
set_target_properties(io PROPERTIES INTERFACE_INCLUDE_DIRECTORIES
                                    ${CMAKE_CURRENT_SOURCE_DIR}/../include)
target_link_libraries(io INTERFACE entity Boost::graph Boost::property_tree)

target_sources(io INTERFACE dfg_io.cpp architecture_io.cpp) 
