#include <Velastic_buffer.h>
#include <gtest/gtest.h>
#include <verilated.h>

#include <iostream>

TEST(VerilogSimulatorTest, elastic_buffer_test) {
  int time_counter = 0;
  Velastic_buffer* elastic_buffer = new Velastic_buffer();

  elastic_buffer->reset_n = 0;
  elastic_buffer->clk = 0;
  elastic_buffer->stop_output = 1;
  elastic_buffer->valid_input = 0;

  while (time_counter < 100) {
    elastic_buffer->eval();
    time_counter++;
  }

  elastic_buffer->reset_n = 1;

  elastic_buffer->clk = 1;
  int cycle = 0;
  while (time_counter < 220) {
    if ((time_counter % 5) == 0) {
      elastic_buffer->clk = !elastic_buffer->clk;
    }

    if ((time_counter % 10) == 0) {
      cycle++;
      if (1 <= cycle && cycle <= 6) {
        elastic_buffer->valid_input = 1;
        elastic_buffer->data_input = cycle;
        EXPECT_EQ(elastic_buffer->DEBUG_data_size, std::min(cycle - 1, 4));
      } else if (7 <= cycle && cycle <= 12) {
        elastic_buffer->valid_input = 0;
        elastic_buffer->stop_output = 0;
        if (7 <= cycle && cycle <= 10) {
          EXPECT_EQ(elastic_buffer->data_output, cycle - 6);
        } else {
          EXPECT_EQ(elastic_buffer->data_output, 1);
        }
      }
    }

    elastic_buffer->eval();
    time_counter++;
  }
}
