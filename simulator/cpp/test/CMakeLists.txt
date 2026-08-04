add_executable(cpp_simulator_test simulator_test.cpp)

include(GoogleTest)

target_link_libraries(cpp_simulator_test PRIVATE gtest_main io mapper
                                                 cpp_simulator)
target_compile_definitions(
  cpp_simulator_test
  PRIVATE CPP_SIMULATOR_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data")

add_test(NAME SimulatorTest COMMAND cpp_simulator_test)
