add_library(common_logging INTERFACE IMPORTED GLOBAL)
set_property(TARGET common_logging PROPERTY CXX_STANDARD 17)
set_target_properties(
  common_logging PROPERTIES INTERFACE_INCLUDE_DIRECTORIES
                            ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_options(common_logging INTERFACE -Wall -Wextra -Wno-deprecated)
