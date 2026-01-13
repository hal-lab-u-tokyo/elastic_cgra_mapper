#include <Velastic_join.h>
#include <gtest/gtest.h>
#include <verilated.h>

#include <functional>
#include <iostream>

TEST(VerilogSimulatorTest, elastic_join_test) {
  Velastic_join* elastic_join = new Velastic_join();

  auto SetElasticJoinValue = [&](int* data_input, bool* valid_input,
                                 bool stop_output) {
    elastic_join->stop_output = stop_output;
    for (int i = 0; i < 2; i++) {
      elastic_join->valid_input[i] = valid_input[i];
      elastic_join->data_input[i] = data_input[i];
    }
  };

  int time_counter = 0;
  int cycle = 0;
  while (time_counter < 120) {
    if ((time_counter % 10) == 0) {
      cycle++;
      if (cycle == 1) {
        int data_input[2] = {cycle, cycle * 2};
        bool valid_input[2] = {0, 0};
        SetElasticJoinValue(data_input, valid_input, 0);
      } else if (cycle == 2) {
        EXPECT_EQ(elastic_join->stop_input[0], 1);
        EXPECT_EQ(elastic_join->stop_input[1], 1);
        EXPECT_EQ(elastic_join->valid_output, 0);
        EXPECT_EQ(elastic_join->data_input[0], 1);
        EXPECT_EQ(elastic_join->data_input[1], 2);
      } else if (cycle == 3) {
        int data_input[2] = {cycle, cycle * 2};
        bool valid_input[2] = {0, 1};
        SetElasticJoinValue(data_input, valid_input, 0);
      } else if (cycle == 4) {
        EXPECT_EQ(elastic_join->stop_input[0], 1);
        EXPECT_EQ(elastic_join->stop_input[1], 1);
        EXPECT_EQ(elastic_join->valid_output, 0);
        EXPECT_EQ(elastic_join->data_input[0], 3);
        EXPECT_EQ(elastic_join->data_input[1], 6);
      } else if (cycle == 5) {
        int data_input[2] = {cycle, cycle * 2};
        bool valid_input[2] = {1, 1};
        SetElasticJoinValue(data_input, valid_input, 0);
      } else if (cycle == 6) {
        EXPECT_EQ(elastic_join->stop_input[0], 0);
        EXPECT_EQ(elastic_join->stop_input[1], 0);
        EXPECT_EQ(elastic_join->valid_output, 1);
        EXPECT_EQ(elastic_join->data_input[0], 5);
        EXPECT_EQ(elastic_join->data_input[1], 10);
      } else if (cycle == 7) {
        int data_input[2] = {cycle, cycle * 2};
        bool valid_input[2] = {0, 0};
        SetElasticJoinValue(data_input, valid_input, 1);
      } else if (cycle == 8) {
        EXPECT_EQ(elastic_join->stop_input[0], 1);
        EXPECT_EQ(elastic_join->stop_input[1], 1);
        EXPECT_EQ(elastic_join->valid_output, 0);
      } else if (cycle == 9) {
        int data_input[2] = {cycle, cycle * 2};
        bool valid_input[2] = {0, 1};
        SetElasticJoinValue(data_input, valid_input, 1);
      } else if (cycle == 10) {
        EXPECT_EQ(elastic_join->stop_input[0], 1);
        EXPECT_EQ(elastic_join->stop_input[1], 1);
        EXPECT_EQ(elastic_join->valid_output, 0);
      } else if (cycle == 11) {
        int data_input[2] = {cycle, cycle * 2};
        bool valid_input[2] = {1, 1};
        SetElasticJoinValue(data_input, valid_input, 1);
      } else if (cycle == 12) {
        EXPECT_EQ(elastic_join->stop_input[0], 1);
        EXPECT_EQ(elastic_join->stop_input[1], 1);
        EXPECT_EQ(elastic_join->valid_output, 1);
      }

      //   std::cout << "cycle: " << cycle << std::endl;
      //   std::cout << "input data/valid/stop: ";
      //   std::cout << elastic_join->data_input[0] << " "
      //             << elastic_join->data_input[1] << " / ";
      //   std::cout << elastic_join->valid_input[0] + 0 << " "
      //             << elastic_join->valid_input[1] + 0 << " / ";
      //   std::cout << elastic_join->stop_input[0] + 0 << " "
      //             << elastic_join->stop_input[1] + 0 << std::endl;
      //   std::cout << "output data/valid/stop: ";
      //   std::cout << elastic_join->data_output[0] + 0 << " "
      //             << elastic_join->data_output[1] + 0 << " / "
      //             << elastic_join->valid_output + 0 << " / "
      //             << elastic_join->stop_output + 0 << std::endl;
    }

    elastic_join->eval();
    time_counter++;
  }
}
