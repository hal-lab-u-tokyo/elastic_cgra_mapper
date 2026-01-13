#include <Velastic_PE.h>
#include <gtest/gtest.h>
#include <verilated.h>

#include <functional>
#include <iostream>

struct PEConfig {
  int input_index_1;
  int input_index_2;
  int output_PE_index[5];
  int op;
  int const_data;

  int GetOutputPEIndexBinary() const {
    int result = 0;

    return 0;
  }
};

void WritePEConfig(int index, int input_index_1, int input_index_2,
                   bool* output_PE_index, int op, int const_data,
                   Velastic_PE* pe) {
  pe->config_index = index;
  pe->config_input_PE_index_1 = input_index_1;
  pe->config_input_PE_index_2 = input_index_2;
  int output_PE_index_binary = 0;
  for (int i = 3; i >= 0; i--) {
    output_PE_index_binary *= 2;
    output_PE_index_binary += output_PE_index[i];
  }
  pe->config_output_PE_index = output_PE_index_binary;
  pe->config_op = op;
  pe->config_const_data = const_data;
  pe->write_config_data = 1;
}

TEST(VerilogSimulatorTest, elastic_PE_test) {
  Velastic_PE* pe = new Velastic_PE();
  const int kNeighborPENum = 4;

  int time_counter = 0;
  pe->reset_n = 0;
  pe->clk = 0;
  pe->mapping_context_max_id = 1;
  while (time_counter < 100) {
    pe->eval();
    time_counter++;
  }

  pe->reset_n = 1;
  pe->mapping_context_max_id = 3;

  int cycle = 0;
  while (time_counter < 570) {
    if ((time_counter % 5) == 0) {
      pe->clk = !pe->clk;
    }
    if ((time_counter % 10) == 0) {
      cycle++;
      if (cycle == 1) {
        bool output_PE_index[kNeighborPENum] = {0, 0, 1, 1};
        WritePEConfig(0, 0, 1, output_PE_index, 1, 0, pe);  // set add config
      } else if (cycle == 2) {
        bool output_PE_index[kNeighborPENum] = {1, 1, 1, 1};
        WritePEConfig(1, 2, 3, output_PE_index, 3, 0, pe);  // set mul config
      } else if (cycle == 3) {
        bool output_PE_index[kNeighborPENum] = {1, 0, 0, 1};
        pe->memory_read_data = 100;
        WritePEConfig(2, 4, 3, output_PE_index, 6, 0, pe);  // set load config
      } else if (cycle == 4) {
        bool output_PE_index[kNeighborPENum] = {1, 0, 0, 0};
        WritePEConfig(3, 0, 2, output_PE_index, 5, 10, pe);  // set const config
        pe->start_exec = 1;
      } else if (cycle == 5) {
        pe->start_exec = 0;
        bool valid_input[kNeighborPENum] = {1, 1, 1, 1};
        for (int i = 0; i < kNeighborPENum; i++) {
          pe->valid_input[i] = 1;
          pe->pe_input_data[i] = i;
        }
      } else if (cycle == 7) {  // add
        bool output_PE_index[kNeighborPENum] = {0, 0, 1, 1};
        for (int i = 0; i < kNeighborPENum; i++) {
          EXPECT_EQ(pe->pe_output_data[i], 1);
          EXPECT_EQ(pe->valid_output[i], output_PE_index[i]);
        }
      } else if (cycle == 12) {  // mul
        bool output_PE_index[kNeighborPENum] = {1, 1, 1, 1};
        for (int i = 0; i < kNeighborPENum; i++) {
          EXPECT_EQ(pe->pe_output_data[i], 6);
          EXPECT_EQ(pe->valid_output[i], output_PE_index[i]);
        }
      } else if (cycle == 17) {  // load
        bool output_PE_index[kNeighborPENum] = {1, 0, 0, 1};
        for (int i = 0; i < kNeighborPENum; i++) {
          EXPECT_EQ(pe->pe_output_data[i], 100);
          EXPECT_EQ(pe->valid_output[i], output_PE_index[i]);
        }
      } else if (cycle == 19) {  // const
        bool output_PE_index[kNeighborPENum] = {1, 0, 0, 0};
        for (int i = 0; i < kNeighborPENum; i++) {
          EXPECT_EQ(pe->pe_output_data[i], 10);
          EXPECT_EQ(pe->valid_output[i], output_PE_index[i]);
        }
      } else if (cycle == 20) {  // mul
        pe->valid_input[2] = 0;
      } else if (cycle == 21) {  // add
        bool output_PE_index[kNeighborPENum] = {0, 0, 1, 1};
        for (int i = 0; i < kNeighborPENum; i++) {
          EXPECT_EQ(pe->pe_output_data[i], 1);
          EXPECT_EQ(pe->valid_output[i], output_PE_index[i]);
        }
      } else if (26 <= cycle && cycle <= 28) {  // mul
        for (int i = 0; i < kNeighborPENum; i++) {
          EXPECT_EQ(pe->valid_output[i], 0);
        }
      } else if (cycle == 29) {
        pe->valid_input[2] = 1;
      } else if (cycle == 34) {  // mul
        for (int i = 0; i < kNeighborPENum; i++) {
          EXPECT_EQ(pe->valid_output[i], 1);
          EXPECT_EQ(pe->pe_output_data[i], 6);
        }
      } else if (cycle == 36) {
        pe->stop_output[0] = 1;
      } else if (37 <= cycle && cycle <= 43) {
        for (int i = 0; i < kNeighborPENum; i++) {
          EXPECT_EQ(pe->pe_output_data[i], 100);
        }
        if (cycle == 43) {
          pe->stop_output[0] = 0;
        }
      } else if (cycle == 44) {
        for (int i = 0; i < kNeighborPENum; i++) {
          EXPECT_EQ(pe->pe_output_data[i], 10);
        }
      }

      std::cout << std::endl;
      std::cout << "<<<<<< cycle " << cycle << " >>>>>>" << std::endl;
      for (int i = 0; i < kNeighborPENum; i++) {
        std::cout << "output_data[" << i << "]: " << pe->pe_output_data[i]
                  << std::endl;
        std::cout << "valid_output[" << i << "]: " << pe->valid_output[i] + 0
                  << std::endl;
      }
      std::cout << "memory_read_address: " << pe->memory_read_address
                << std::endl;
    }

    pe->eval();
    time_counter++;
  }
}

TEST(VerilogSimulatorTest, elastic_PE_test_const) {
  Velastic_PE* pe = new Velastic_PE();
  const int kNeighborPENum = 4;

  int time_counter = 0;
  pe->reset_n = 0;
  pe->clk = 0;
  pe->mapping_context_max_id = 1;
  while (time_counter < 100) {
    pe->eval();
    time_counter++;
  }

  pe->reset_n = 1;
  pe->mapping_context_max_id = 1;

  int cycle = 0;
  while (time_counter < 220) {
    if ((time_counter % 5) == 0) {
      pe->clk = !pe->clk;
    }
    if ((time_counter % 10) == 0) {
      cycle++;
      if (cycle == 1) {
        bool output_PE_index[kNeighborPENum] = {1, 0, 0, 0};
        WritePEConfig(0, 0, 2, output_PE_index, 5, 10, pe);  // set const config
      }
      if (cycle == 2) {
        bool output_PE_index[kNeighborPENum] = {1, 0, 0, 0};
        WritePEConfig(1, 0, 2, output_PE_index, 5, 100,
                      pe);  // set const config
        pe->start_exec = 1;
      } else if (cycle == 3) {
        pe->start_exec = 0;
        pe->stop_output[0] = 1;
      } else if (5 <= cycle) {
        EXPECT_EQ(pe->pe_output_data[0], 10);
        EXPECT_EQ(pe->valid_output[0], 1);
      }

      //   std::cout << std::endl;
      //   std::cout << "<<<<<< cycle " << cycle << " >>>>>>" << std::endl;
      //   for (int i = 0; i < kNeighborPENum; i++) {
      //     std::cout << "output_data[" << i << "]: " << pe->pe_output_data[i]
      //               << std::endl;
      //     std::cout << "valid_output[" << i << "]: " << pe->valid_output[i] +
      //     0
      //               << std::endl;
      //   }
      //   std::cout << "memory_read_address: " << pe->memory_read_address
      //             << std::endl;
    }

    pe->eval();
    time_counter++;
  }
}

TEST(VerilogSimulatorTest, elastic_PE_test_route) {
  Velastic_PE* pe = new Velastic_PE();
  const int kNeighborPENum = 4;

  int time_counter = 0;
  pe->reset_n = 1;
  pe->clk = 0;
  pe->mapping_context_max_id = 1;
  while (time_counter < 100) {
    if (time_counter == 1) {
      pe->reset_n = 0;
    }
    pe->eval();
    time_counter++;
  }

  pe->reset_n = 1;
  pe->mapping_context_max_id = 0;

  int cycle = 0;
  while (time_counter < 170) {
    if ((time_counter % 5) == 0) {
      pe->clk = !pe->clk;
    }
    if ((time_counter % 10) == 0) {
      cycle++;
      if (cycle == 1) {
        bool output_PE_index[kNeighborPENum] = {1, 0, 0, 0};
        WritePEConfig(0, 0, 2, output_PE_index, 8, 10, pe);  // set const config
        pe->start_exec = 1;
      } else if (cycle == 2) {
        pe->start_exec = 0;
        pe->stop_output[0] = 1;
      } else if (5 <= cycle) {
        EXPECT_EQ(pe->pe_output_data[0], 0);
        EXPECT_EQ(pe->valid_output[0], 1);
      }

      //   std::cout << std::endl;
      // std::cout << "<<<<<< cycle " << cycle << " >>>>>>" << std::endl;
      //   for (int i = 0; i < kNeighborPENum; i++) {
      //     std::cout << "output_data[" << i << "]: " << pe->pe_output_data[i]
      //               << std::endl;
      //     std::cout << "valid_output[" << i << "]: " << pe->valid_output[i] +
      //     0
      //               << std::endl;
      //   }
      //   std::cout << "memory_read_address: " << pe->memory_read_address
      //             << std::endl;
    }

    pe->eval();
    time_counter++;
  }
}

TEST(VerilogSimulatorTest, elastic_PE_test_initialize) {
  Velastic_PE* pe = new Velastic_PE();
  const int kNeighborPENum = 4;

  int time_counter = 0;
  pe->reset_n = 0;
  pe->clk = 0;
  pe->mapping_context_max_id = 1;
  while (time_counter < 100) {
    pe->eval();
    time_counter++;
  }

  pe->reset_n = 1;
  pe->mapping_context_max_id = 0;

  int cycle = 0;
  while (time_counter < 150) {
    if ((time_counter % 5) == 0) {
      pe->clk = !pe->clk;
    }
    if ((time_counter % 10) == 0) {
      cycle++;
      if (cycle == 1) {
        pe->start_exec = 1;
        pe->pe_input_data[0] = 4;
        bool output_PE_index[kNeighborPENum] = {1, 0, 0, 0};
        WritePEConfig(0, 0, 4, output_PE_index, 1, 10, pe);  // set const config
      } else if (cycle == 2) {
        pe->start_exec = 0;
        pe->valid_input[0] = 1;
      } else if (4 == cycle) {
        EXPECT_EQ(pe->pe_output_data[0], 4);
        EXPECT_EQ(pe->valid_output[0], 1);
        EXPECT_EQ(pe->stop_input[0], 0);
      }

      // std::cout << std::endl;
      // std::cout << "<<<<<< cycle " << cycle << " >>>>>>" << std::endl;
      // for (int i = 0; i < kNeighborPENum; i++) {
      //   std::cout << "output_data[" << i << "]: " << pe->pe_output_data[i]
      //             << std::endl;
      //   std::cout << "valid_output[" << i << "]: " << pe->valid_output[i] + 0
      //             << std::endl;
      // }
      // std::cout << "memory_read_address: " << pe->memory_read_address
      //           << std::endl;
    }

    pe->eval();
    time_counter++;
  }
}
