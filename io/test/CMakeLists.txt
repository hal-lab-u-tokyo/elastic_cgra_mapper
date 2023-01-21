add_executable(io_test dfg_io_test.cpp)

# find_package(GTest REQUIRED)
include(GoogleTest)

target_link_libraries(io_test PRIVATE gtest_main io)

add_test(NAME IOTest COMMAND io_test)
