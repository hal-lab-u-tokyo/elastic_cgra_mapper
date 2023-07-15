#include <Velastic_multiplexer.h>
#include <gtest/gtest.h>
#include <verilated.h>

#include <functional>
#include <iostream>

TEST(VerilogSimulatorTest, elastic_mux_test) {
  Velastic_multiplexer* elastic_mux = new Velastic_multiplexer();

  auto SetElasticMuxValue = [&](int* data_input, bool* valid_input,
                                bool stop_output, int data_index) {
    elastic_mux->stop_output = stop_output;
    elastic_mux->input_data_index = data_index;
    for (int i = 0; i < 5; i++) {
      elastic_mux->valid_input[i] = valid_input[i];
      elastic_mux->data_input[i] = data_input[i];
    }
  };

  int time_counter = 0;
  int cycle = 0;
  while (time_counter < 120) {
    if ((time_counter % 10) == 0) {
      cycle++;
      if (cycle == 1) {
        int data_input[5] = {1, 2, 3, 4, 5};
        bool valid_input[5] = {1, 1, 1, 1, 1};
        SetElasticMuxValue(data_input, valid_input, 0, 0);
      } else if (cycle == 2) {
        EXPECT_EQ(elastic_mux->data_output, 1);
        EXPECT_EQ(elastic_mux->valid_output, 1);
        for (int i = 0; i < 5; i++) {
          EXPECT_EQ(elastic_mux->stop_input[i], 0);
        }
      } else if (cycle == 3) {
        int data_input[5] = {1, 2, 3, 4, 5};
        bool valid_input[5] = {1, 1, 0, 1, 1};
        SetElasticMuxValue(data_input, valid_input, 0, 1);
      } else if (cycle == 4) {
        EXPECT_EQ(elastic_mux->data_output, 2);
        EXPECT_EQ(elastic_mux->valid_output, 1);
        for (int i = 0; i < 5; i++) {
          EXPECT_EQ(elastic_mux->stop_input[i], 0);
        }
      } else if (cycle == 5) {
        int data_input[5] = {1, 2, 3, 4, 5};
        bool valid_input[5] = {0, 0, 0, 0, 0};
        SetElasticMuxValue(data_input, valid_input, 0, 4);
      } else if (cycle == 6) {
        EXPECT_EQ(elastic_mux->data_output, 5);
        EXPECT_EQ(elastic_mux->valid_output, 0);
        for (int i = 0; i < 5; i++) {
          EXPECT_EQ(elastic_mux->stop_input[i], 0);
        }
      } else if (cycle == 7) {
        int data_input[5] = {1, 2, 3, 4, 5};
        bool valid_input[5] = {1, 1, 1, 1, 1};
        SetElasticMuxValue(data_input, valid_input, 1, 2);
      } else if (cycle == 8) {
        EXPECT_EQ(elastic_mux->data_output, 3);
        EXPECT_EQ(elastic_mux->valid_output, 1);
        for (int i = 0; i < 5; i++) {
          EXPECT_EQ(elastic_mux->stop_input[i], 1);
        }
      } else if (cycle == 9) {
        int data_input[5] = {1, 2, 3, 4, 5};
        bool valid_input[5] = {1, 0, 1, 1, 0};
        SetElasticMuxValue(data_input, valid_input, 1, 3);
      } else if (cycle == 10) {
        EXPECT_EQ(elastic_mux->data_output, 4);
        EXPECT_EQ(elastic_mux->valid_output, 1);
        for (int i = 0; i < 5; i++) {
          EXPECT_EQ(elastic_mux->stop_input[i], 1);
        }
      } else if (cycle == 11) {
        int data_input[5] = {1, 2, 3, 4, 5};
        bool valid_input[5] = {0, 0, 0, 0, 0};
        SetElasticMuxValue(data_input, valid_input, 1, 4);
      } else if (cycle == 12) {
        EXPECT_EQ(elastic_mux->data_output, 5);
        EXPECT_EQ(elastic_mux->valid_output, 0);
        for (int i = 0; i < 5; i++) {
          EXPECT_EQ(elastic_mux->stop_input[i], 1);
        }
      }

      //   std::cout << "cycle: " << cycle << std::endl;
      //   std::cout << "data input: ";
      //   for (int i = 0; i < 5; i++) {
      //     std::cout << elastic_mux->data_input[i] << " ";
      //   }
      //   std::cout << std::endl;
      //   std::cout << "valid input: ";
      //   for (int i = 0; i < 5; i++) {
      //     std::cout << elastic_mux->valid_input[i] + 0 << " ";
      //   }
      //   std::cout << std::endl;
      //   std::cout << "stop input: ";
      //   for (int i = 0; i < 5; i++) {
      //     std::cout << elastic_mux->stop_input[i] + 0 << " ";
      //   }
      //   std::cout << std::endl;
      //   std::cout << "input data index: " << elastic_mux->input_data_index +
      //   0
      //             << std::endl;
      //   std::cout << "output data/valid/stop: " << elastic_mux->data_output
      //   << "/"
      //             << elastic_mux->valid_output + 0 << "/"
      //             << elastic_mux->stop_output + 0 << std::endl;
    }

    elastic_mux->eval();
    time_counter++;
  }
}