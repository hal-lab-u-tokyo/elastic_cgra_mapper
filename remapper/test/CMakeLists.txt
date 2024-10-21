add_executable(remapper_test remapper_test.cpp)

include(GoogleTest)

target_link_libraries(remapper_test PRIVATE gtest_main remapper io)

add_test(NAME RemapperTest COMMAND remapper_test)
