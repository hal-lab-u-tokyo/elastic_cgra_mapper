add_executable(cpp_simulator_test simulator_test.cpp)

include(GoogleTest)

target_link_libraries(cpp_simulator_test PRIVATE gtest_main io mapper
                                                 cpp_simulator)

add_test(NAME SimulatorTest COMMAND cpp_simulator_test)
