#include <Vsynchronous_CGRA.h>
#include <gtest/gtest.h>
#include <verilated.h>

#include <io/architecture_io.hpp>
#include <io/dfg_io.hpp>
#include <io/mapping_io.hpp>
#include <iostream>
#include <mapper/gurobi_mapper.hpp>
#include <vector>

TEST(VerilogSimulatorTest, synchronous_CGRA_test) {
  std::string mapping_file_path =
      "../../../../simulator/verilog/test/data/default_mapping.json";
  std::shared_ptr<entity::Mapping> mapping_ptr =
      std::make_shared<entity::Mapping>();
  *mapping_ptr = io::ReadMappingFile(mapping_file_path);

  // verilog simulator
  auto GetInputPEIndex = [](entity::ConfigId from, entity::ConfigId to) {
    if (from.row_id < to.row_id) return 0;
    if (from.row_id > to.row_id) return 1;
    if (from.column_id < to.column_id) return 2;
    if (from.column_id > to.column_id) return 3;
    return 4;
  };

  auto GetOpIndex = [](entity::OpType op) {
    if (op == entity::OpType::NOP) return 0;
    if (op == entity::OpType::ADD) return 1;
    if (op == entity::OpType::SUB) return 2;
    if (op == entity::OpType::MUL) return 3;
    if (op == entity::OpType::DIV) return 4;
    if (op == entity::OpType::CONST) return 5;
    if (op == entity::OpType::LOAD) return 6;
    if (op == entity::OpType::OUTPUT) return 7;
    if (op == entity::OpType::ROUTE) return 8;
    return 0;
  };

  int time_counter = 0;
  Vsynchronous_CGRA* cgra = new Vsynchronous_CGRA();
  const int kCGRAContextCycle = 4;

  cgra->reset_n = 0;
  cgra->clk = 0;

  while (time_counter < 100) {
    cgra->eval();
    time_counter++;
  }

  cgra->reset_n = 1;
  cgra->clk = 1;
  int row_size = mapping_ptr->GetMRRGConfig().row;
  int column_size = mapping_ptr->GetMRRGConfig().column;
  int context_size = mapping_ptr->GetMRRGConfig().context_size;

  int cycle = 0;
  int config_size = row_size * column_size * context_size;
  entity::ConfigId output_config_id;
  std::vector<int> output_vec;
  while (time_counter < 1900) {
    if ((time_counter % 5) == 0) {
      cgra->clk = !cgra->clk;  // Toggle clock
    }
    if ((time_counter % 10) == 0) {
      cycle++;

      if (1 <= cycle && cycle <= config_size) {
        int context_id = (cycle - 1) % context_size;
        int column_id = ((cycle - 1) / context_size) % column_size;
        int row_id = (cycle - 1) / (context_size * column_size);

        entity::ConfigId config_id(row_id, column_id, context_id);
        entity::CGRAConfig cgra_config = mapping_ptr->GetConfig(config_id);

        cgra->write_config_data = 1;
        cgra->config_PE_row_index = row_id;
        cgra->config_PE_column_index = column_id;
        cgra->config_index = context_id;
        cgra->config_input_PE_index_1 =
            GetInputPEIndex(cgra_config.from_config_id_vec[0], config_id);
        cgra->config_input_PE_index_2 =
            GetInputPEIndex(cgra_config.from_config_id_vec[1], config_id);
        cgra->config_op = GetOpIndex(cgra_config.operation_type);
        cgra->config_const_data = cgra_config.const_value;

        if (cgra_config.operation_type == entity::OpType::OUTPUT) {
          output_config_id = config_id;
        }
      } else if (config_size + 1 <= cycle && cycle <= config_size + 20) {
        cgra->write_config_data = 0;
        cgra->memory_write_data = (cycle - config_size - 1);
        cgra->memory_write_address = (cycle - config_size - 1) * 1;
        cgra->memory_write = 1;
      } else if (config_size + 21 <= cycle && cycle <= config_size + 40) {
        cgra->memory_write_data = (cycle - config_size - 21) * 2;
        cgra->memory_write_address = (cycle - config_size - 21) * 30;
        cgra->memory_write = 1;
        if (cycle == config_size + 40) {
          cgra->start_exec = 1;
          cgra->mapping_context_max_id = context_size - 1;
        }
      } else if (config_size + 61 <= cycle && cycle <= config_size + 180) {
        cgra->memory_write = 0;
        cgra->start_exec = 0;
        int exe_cycle = cycle - (config_size + 61);
        int tmp_config_id =
            ((exe_cycle - kCGRAContextCycle) / kCGRAContextCycle) %
            context_size;
        if (exe_cycle % 4 == 0) {
          std::cout << "-- step " << exe_cycle / 4 << " --" << std::endl;
          std::cout << cgra->DEBUG_memory_read_address[1][0] << std::endl;
          std::cout << "output/input1/input2/index1/index2" << std::endl;
          for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 4; j++) {
              std::cout << cgra->pe_output[i][j] << "/"
                        << cgra->DEBUG_input_PE_index_1[i][j] + 0 << "/"
                        << cgra->DEBUG_input_PE_index_2[i][j] + 0 << " ";
            }
            std::cout << std::endl;
          }
        }
        if (exe_cycle % kCGRAContextCycle == 0 &&
            tmp_config_id == output_config_id.context_id) {
          output_vec.push_back(cgra->pe_output[output_config_id.row_id]
                                              [output_config_id.column_id]);
        }
      }
    }

    cgra->eval();
    time_counter++;
  }

  cgra->final();
  for (int i : output_vec) {
    std::cout << i << std::endl;
  }
  int count = 0;
  std::cout << output_vec.size() << std::endl;
  for (int i = 0; i < 7; i++) {
    count += i * i * 2;
    EXPECT_EQ(output_vec[i + 4], count);
  }
}
