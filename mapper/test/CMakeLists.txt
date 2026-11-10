add_executable(mapper_test ilp_mapper_test.cpp configuration_io_test.cpp)

include(GoogleTest)

target_link_libraries(mapper_test PRIVATE gtest_main mapper)

add_test(NAME MapperTest COMMAND mapper_test)

set_source_files_properties(configuration_io_test.cpp PROPERTIES
  COMPILE_DEFINITIONS
  "MAPPER_DEFAULT_CONFIG_FILE=\"${PROJECT_SOURCE_DIR}/data/mapper_config.json\"")
