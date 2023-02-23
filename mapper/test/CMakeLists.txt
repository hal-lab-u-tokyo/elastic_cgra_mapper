add_executable(mapper_test gurobi_mapper_test.cpp)

include(GoogleTest)

target_link_libraries(mapper_test PRIVATE gtest_main mapper)

add_test(NAME MapperTest COMMAND mapper_test)