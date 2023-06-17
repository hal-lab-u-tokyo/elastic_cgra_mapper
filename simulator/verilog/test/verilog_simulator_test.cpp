#include <Vdata_memory.h>
#include <gtest/gtest.h>
#include <verilated.h>

TEST(VerilogSimulatorTest, data_memory_test) {
  int time_counter = 0;
  Vdata_memory *data_memory = new Vdata_memory();

  data_memory->reset_n = 0;
  data_memory->clk = 0;

  while (time_counter < 100) {
    data_memory->eval();
    time_counter++;
  }

  data_memory->reset_n = 1;

  int cycle = 0;
  while (time_counter < 200) {
    if ((time_counter % 5) == 0) {
      data_memory->clk = !data_memory->clk;  // Toggle clock
    }
    if ((time_counter % 10) == 0) {
      cycle++;
      if (cycle == 1) {
        data_memory->address = 10;
      } else if (cycle == 2) {
        data_memory->address = 25;
        data_memory->write = 1;
        data_memory->input_data = 225;
        EXPECT_EQ(0, data_memory->output_data);
      } else if (cycle == 3) {
        data_memory->write = 0;
        data_memory->address = 25;
      } else if (cycle == 4) {
        EXPECT_EQ(225, data_memory->output_data);
      }
    }

    data_memory->eval();
    time_counter++;
  }
}