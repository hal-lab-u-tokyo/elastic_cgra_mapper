add_executable(simulator_test simulator_test.cpp)

include(GoogleTest)

target_link_libraries(simulator_test PRIVATE gtest_main io mapper simulator)

add_test(NAME SimulatorTest COMMAND simulator_test)
