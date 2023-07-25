#include <Velastic_fork.h>
#include <gtest/gtest.h>
#include <verilated.h>

#include <functional>
#include <iostream>

TEST(VerilogSimulatorTest, elastic_fork_test) {
  int time_counter = 0;
  Velastic_fork* elastic_fork = new Velastic_fork();
  const int kNeighborPENum = 4;

  auto SetElasticForkValue = [&](int input_data, bool valid_input,
                                 bool* stop_output, bool* available_output) {
    elastic_fork->input_data = input_data;
    elastic_fork->valid_input = valid_input;
    for (int i = 0; i < kNeighborPENum; i++) {
      elastic_fork->stop_output[i] = stop_output[i];
    }
    int available_output_binary = 0;
    for (int i = kNeighborPENum - 1; i >= 0; i--) {
      available_output_binary *= 2;
      available_output_binary += available_output[i];
    }
    elastic_fork->available_output = available_output_binary;
  };

  elastic_fork->reset_n = 0;
  elastic_fork->clk = 0;
  elastic_fork->valid_input = 0;

  while (time_counter < 100) {
    elastic_fork->eval();
    time_counter++;
  }

  elastic_fork->reset_n = 1;
  elastic_fork->clk = 1;
  int cycle = 0;
  while (time_counter < 220) {
    if ((time_counter % 5) == 0) {
      elastic_fork->clk = !elastic_fork->clk;
    }

    if ((time_counter % 10) == 0) {
      cycle++;
      if (cycle == 1) {
        elastic_fork->stop_input = 1;
        bool stop_output[kNeighborPENum] = {0, 0, 0, 0};
        bool available_output[kNeighborPENum] = {1, 1, 1, 1};
        SetElasticForkValue(cycle, 1, stop_output, available_output);
      } else if (cycle == 2) {
        EXPECT_EQ(elastic_fork->stop_input, 0);
        for (int i = 0; i < 4; i++) {
          EXPECT_EQ(elastic_fork->valid_output[i], 1);
          EXPECT_EQ(elastic_fork->output_data[i], 1);
          EXPECT_EQ(elastic_fork->switch_context, 1);
        }
      } else if (cycle == 3) {
        bool stop_output[kNeighborPENum] = {0, 0, 1, 0};
        bool available_output[kNeighborPENum] = {1, 1, 1, 1};
        SetElasticForkValue(cycle, 1, stop_output, available_output);
      } else if (cycle == 4) {
        EXPECT_EQ(elastic_fork->stop_input, 1);
        bool valid_output_result[kNeighborPENum] = {0, 0, 1, 0};
        for (int i = 0; i < 4; i++) {
          EXPECT_EQ(elastic_fork->valid_output[i], valid_output_result[i]);
        }
        EXPECT_EQ(elastic_fork->switch_context, 0);
      } else if (cycle == 5) {
        bool stop_output[kNeighborPENum] = {0, 0, 0, 0};
        bool available_output[kNeighborPENum] = {1, 1, 1, 1};
        SetElasticForkValue(cycle, 0, stop_output, available_output);
      } else if (cycle == 6) {
        EXPECT_EQ(elastic_fork->stop_input, 0);
        for (int i = 0; i < 4; i++) {
          EXPECT_EQ(elastic_fork->valid_output[i], 0);
        }
        EXPECT_EQ(elastic_fork->switch_context, 0);
      } else if (cycle == 7) {
        bool stop_output[kNeighborPENum] = {0, 0, 1, 0};
        bool available_output[kNeighborPENum] = {1, 1, 1, 1};
        SetElasticForkValue(cycle, 0, stop_output, available_output);
      } else if (cycle == 8) {
        EXPECT_EQ(elastic_fork->stop_input, 1);
        for (int i = 0; i < 4; i++) {
          EXPECT_EQ(elastic_fork->valid_output[i], 0);
        }
        EXPECT_EQ(elastic_fork->switch_context, 0);
      } else if (cycle == 9) {
        bool stop_output[kNeighborPENum] = {0, 0, 1, 0};
        bool available_output[kNeighborPENum] = {1, 1, 0, 1};
        SetElasticForkValue(cycle, 1, stop_output, available_output);
      } else if (cycle == 10) {
        bool stop_output[kNeighborPENum] = {0, 0, 1, 0};
        EXPECT_EQ(elastic_fork->stop_input, 0);
        for (int i = 0; i < 4; i++) {
          EXPECT_EQ(elastic_fork->valid_output[i], !stop_output[i]);
        }
        EXPECT_EQ(elastic_fork->switch_context, 1);
      } else if (cycle == 11) {
        bool stop_output[kNeighborPENum] = {0, 0, 1, 0};
        bool available_output[kNeighborPENum] = {1, 1, 0, 1};
        SetElasticForkValue(cycle, 0, stop_output, available_output);
      } else if (cycle == 12) {
        EXPECT_EQ(elastic_fork->stop_input, 0);
        for (int i = 0; i < 4; i++) {
          EXPECT_EQ(elastic_fork->valid_output[i], 0);
        }
        EXPECT_EQ(elastic_fork->switch_context, 0);
      }

      //   std::cout << "---" << cycle << "---" << std::endl;
      //   std::cout << "input data/valid/stop: " << elastic_fork->input_data <<
      //   " "
      //             << elastic_fork->valid_input + 0 << " "
      //             << elastic_fork->stop_input + 0 << std::endl;
      //   std::cout << "output data: ";
      //   for (int i = 0; i < 5; i++) {
      //     std::cout << elastic_fork->output_data[i] << " ";
      //   }
      //   std::cout << std::endl;
      //   std::cout << "valid output: ";
      //   for (int i = 0; i < 5; i++) {
      //     std::cout << elastic_fork->valid_output[i] + 0 << " ";
      //   }
      //   std::cout << std::endl;
      //   std::cout << "stop output: ";
      //   for (int i = 0; i < 5; i++) {
      //     std::cout << elastic_fork->stop_output[i] + 0 << " ";
      //   }
      //   std::cout << std::endl;
    }

    elastic_fork->eval();
    time_counter++;
  }
}