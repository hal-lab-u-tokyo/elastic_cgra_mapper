#include <Velastic_fork.h>
#include <gtest/gtest.h>
#include <verilated.h>

#include <functional>
#include <iostream>

TEST(VerilogSimulatorTest, elastic_fork_test) {
  int time_counter = 0;
  Velastic_fork* elastic_fork = new Velastic_fork();

  auto SetElasticForkValue = [&](int input_data, bool valid_input,
                                 bool* stop_output) {
    elastic_fork->input_data = input_data;
    elastic_fork->valid_input = valid_input;
    for (int i = 0; i < 5; i++) {
      elastic_fork->stop_output[i] = stop_output[i];
    }
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
  while (time_counter < 200) {
    if ((time_counter % 5) == 0) {
      elastic_fork->clk = !elastic_fork->clk;
    }

    if ((time_counter % 10) == 0) {
      cycle++;
      if (cycle == 1) {
        bool stop_output[5] = {0, 0, 0, 0, 0};
        SetElasticForkValue(cycle, 1, stop_output);
      } else if (cycle == 2) {
        EXPECT_EQ(elastic_fork->stop_input, 0);
        for (int i = 0; i < 5; i++) {
          EXPECT_EQ(elastic_fork->valid_output[i], 1);
          EXPECT_EQ(elastic_fork->output_data[i], 1);
        }
      } else if (cycle == 3) {
        bool stop_output[5] = {0, 0, 1, 0, 1};
        SetElasticForkValue(cycle, 1, stop_output);
      } else if (cycle == 4) {
        EXPECT_EQ(elastic_fork->stop_input, 1);
        bool valid_output_result[5] = {0, 0, 1, 0, 1};
        for (int i = 0; i < 5; i++) {
          EXPECT_EQ(elastic_fork->valid_output[i], valid_output_result[i]);
        }
      } else if (cycle == 5) {
        bool stop_output[5] = {0, 0, 0, 0, 0};
        SetElasticForkValue(cycle, 0, stop_output);
      } else if (cycle == 6) {
        EXPECT_EQ(elastic_fork->stop_input, 0);
        for (int i = 0; i < 5; i++) {
          EXPECT_EQ(elastic_fork->valid_output[i], 0);
        }
      } else if (cycle == 7) {
        bool stop_output[5] = {0, 0, 1, 0, 1};
        SetElasticForkValue(cycle, 0, stop_output);
      } else if (cycle == 8) {
        EXPECT_EQ(elastic_fork->stop_input, 1);
        for (int i = 0; i < 5; i++) {
          EXPECT_EQ(elastic_fork->valid_output[i], 0);
        }
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