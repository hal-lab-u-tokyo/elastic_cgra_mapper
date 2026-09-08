add_executable(remapper_test remapper_test.cpp)

include(GoogleTest)

target_link_libraries(remapper_test PRIVATE gtest_main remapper io)

target_compile_definitions(
  remapper_test
  PRIVATE
    REMAPPER_TEST_DATABASE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data/database/")

add_test(NAME RemapperTest COMMAND remapper_test)
