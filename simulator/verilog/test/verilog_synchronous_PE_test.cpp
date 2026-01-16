#include <Vsynchronous_PE.h>
#include <gtest/gtest.h>
#include <verilated.h>

TEST(VerilogSimulatorTest, synchronous_PE_test) {
  int time_counter = 0;
  Vsynchronous_PE *pe = new Vsynchronous_PE();

  pe->reset_n = 0;
  pe->clk = 0;

  while (time_counter < 100) {
    pe->eval();
    time_counter++;
  }

  pe->reset_n = 1;

  int cycle = 0;
  while (time_counter < 550) {
    if ((time_counter % 5) == 0) {
      pe->clk = !pe->clk;  // Toggle clock
    }
    if ((time_counter % 10) == 0) {
      cycle++;
      if (1 <= cycle && cycle <= 8) {
        pe->write_config_data = 1;
        pe->config_index = cycle - 1;
        pe->config_input_PE_index_1 = (cycle - 1) % 4;
        pe->config_input_PE_index_2 = cycle % 4;
        pe->config_const_data = cycle * 2;
        pe->config_op = cycle;
        pe->mapping_context_max_id = 7;
      } else if (cycle == 11) {
        pe->write_config_data = 0;
        pe->start_exec = 1;
      } else if (cycle == 12) {  // 0, 1, ADD
        pe->start_exec = 0;
        pe->pe_input_data[0] = 12;
        pe->pe_input_data[1] = 13;
      } else if (cycle == 16) {  // 1, 2, SUB
        EXPECT_EQ(pe->pe_output_data, 25);
        pe->pe_input_data[1] = 20;
        pe->pe_input_data[2] = 15;
      } else if (cycle == 20) {  // 2, 3, MUL
        EXPECT_EQ(pe->pe_output_data, 5);
        pe->pe_input_data[2] = 4;
        pe->pe_input_data[3] = 15;
      } else if (cycle == 24) {  // 3, 0, DIV
        EXPECT_EQ(pe->pe_output_data, 60);
        pe->pe_input_data[3] = 62;
        pe->pe_input_data[0] = 2;
      } else if (cycle == 28) {  // 0, 1, CONST
        EXPECT_EQ(pe->pe_output_data, 31);
        pe->pe_input_data[0] = 0;
      } else if (cycle == 32) {  // 1, 2, load
        EXPECT_EQ(pe->pe_output_data, 10);
        pe->pe_input_data[1] = 20;
        pe->pe_input_data[2] = 15;
      } else if (cycle == 36) {  // 2, 3, output
        EXPECT_EQ(pe->memory_read_address, 20);
        pe->pe_input_data[2] = 15;
      } else if (cycle == 40) {  // 3, 0, route
        EXPECT_EQ(pe->pe_output_data, 15);
        pe->pe_input_data[3] = 20;
      } else if (cycle == 44) {
        EXPECT_EQ(pe->pe_output_data, 20);
      }
    }

    pe->eval();
    time_counter++;
  }
}
