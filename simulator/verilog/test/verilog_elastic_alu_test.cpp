// #include <Velastic_PE.h>
#include <Velastic_alu.h>
#include <gtest/gtest.h>
#include <verilated.h>

#include <functional>
#include <iostream>

void SetOperation(int data_1, int data_2, int op, int const_data,
                  bool valid_input, bool stop_output, Velastic_alu* alu) {
  alu->input_data_1 = data_1;
  alu->input_data_2 = data_2;
  alu->op = op;
  alu->const_data = const_data;
  alu->valid_input = valid_input;
  alu->stop_output = stop_output;
}

void EvaluateOutput(int valid_output, int stop_input, int output_data,
                    int memory_write_address, int memory_write,
                    int memory_write_data, int memory_read_address,
                    Velastic_alu* alu) {
  if (valid_output >= 0) {
    EXPECT_EQ(alu->valid_output, valid_output);
  }
  if (stop_input >= 0) {
    EXPECT_EQ(alu->stop_input, stop_input);
  }
  if (output_data >= 0) {
    EXPECT_EQ(alu->output_data, output_data);
  }
  if (memory_write_address >= 0) {
    EXPECT_EQ(alu->memory_write_address, memory_write_address);
  }
  if (memory_write >= 0) {
    EXPECT_EQ(alu->memory_write, memory_write);
  }
  if (memory_write_data >= 0) {
    EXPECT_EQ(alu->memory_write_data, memory_write_data);
  }
  if (memory_read_address >= 0) {
    EXPECT_EQ(alu->memory_read_address, memory_read_address);
  }
}

TEST(VerilogSimulatorTest, elastic_alu_test) {
  Velastic_alu* alu = new Velastic_alu();

  int time_counter = 0;
  alu->reset_n = 0;
  alu->clk = 0;
  while (time_counter < 100) {
    alu->eval();
    time_counter++;
  }

  alu->reset_n = 1;
  alu->clk = 1;
  int cycle = 0;
  while (time_counter < 370) {
    if ((time_counter % 5) == 0) {
      alu->clk = !alu->clk;
    }
    if ((time_counter % 10) == 0) {
      cycle++;
      if (cycle == 1) {
        SetOperation(1, 2, 1, 0, 1, 0, alu);  // add
      } else if (cycle == 2) {
        alu->valid_input = 0;
        EvaluateOutput(1, 1, 3, -1, -1, -1, -1, alu);
      } else if (cycle == 3) {
        SetOperation(6, 2, 2, 0, 0, 0, alu);  // sub
      } else if (cycle == 4) {
        alu->valid_input = 0;
        EvaluateOutput(0, 0, -1, -1, -1, -1, -1, alu);
      } else if (cycle == 5) {
        alu->valid_input = 1;
      } else if (cycle == 6) {
        alu->valid_input = 0;
        EvaluateOutput(1, 1, 4, -1, -1, -1, -1, alu);
      } else if (cycle == 7) {
        SetOperation(3, 4, 3, 0, 1, 0, alu);  // mul
      } else if (cycle == 9) {
        EvaluateOutput(0, 1, 12, -1, -1, -1, -1, alu);
      } else if (cycle == 11) {
        alu->valid_input = 0;
        EvaluateOutput(1, 1, 12, -1, -1, -1, -1, alu);
      } else if (cycle == 12) {
        SetOperation(15, 3, 4, 0, 1, 0, alu);  // div
      } else if (cycle == 16) {
        alu->valid_input = 0;
        EvaluateOutput(1, 1, 5, -1, -1, -1, -1, alu);
      } else if (cycle == 17) {
        SetOperation(15, 3, 5, 100, 1, 0, alu);  // const
      } else if (cycle == 18) {
        alu->valid_input = 0;
        EvaluateOutput(1, 1, 100, -1, -1, -1, -1, alu);
      } else if (cycle == 19) {
        SetOperation(20, 34, 6, 0, 1, 0, alu);  // load
        alu->memory_read_data = 200;
      } else if (cycle == 23) {
        alu->valid_input = 0;
        EvaluateOutput(1, 1, 200, -1, -1, -1, 20, alu);
      } else if (cycle == 24) {
        SetOperation(15, 3, 7, 0, 1, 0, alu);  // output
      } else if (cycle == 25) {
        alu->valid_input = 0;
        EvaluateOutput(1, 1, 15, -1, -1, -1, -1, alu);
      } else if (cycle == 26) {
        SetOperation(20, 3, 8, 0, 1, 0, alu);  // route
      } else if (cycle == 27) {
        alu->valid_input = 0;
        EvaluateOutput(1, 1, 20, -1, -1, -1, -1, alu);
      }
      // std::cout << "----- cycle " << cycle << " -----" << std::endl;
      // std::cout << "output: " << alu->output_data << std::endl;
      // std::cout << "stop_input: " << alu->stop_input[0] + 0 << std::endl;
      // std::cout << "valid_output: " << alu->valid_output + 0 << std::endl;
      // std::cout << "memory_write_address: " << alu->memory_write_address
      //           << std::endl;
      // std::cout << "memory_write: " << alu->memory_write + 0 << std::endl;
      // std::cout << "memory_write_data: " << alu->memory_write_data <<
      // std::endl; std::cout << "memory_read_address: " <<
      // alu->memory_read_data
      //           << std::endl;
    }
    alu->eval();
    time_counter++;
  }
}