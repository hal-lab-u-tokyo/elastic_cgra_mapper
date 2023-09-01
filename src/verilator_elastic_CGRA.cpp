#include <Velastic_CGRA.h>
#include <verilated.h>
#include <verilated_vcd_c.h>

#include <io/architecture_io.hpp>
#include <io/dfg_io.hpp>
#include <io/mapping_io.hpp>
#include <iostream>
#include <mapper/gurobi_mapper.hpp>
#include <vector>

int GetOutputPEIndex(entity::ConfigId from,
                     std::vector<entity::ConfigId> to_vec) {
  int from_row_id = from.row_id;
  int from_column_id = from.column_id;

  int output_vec[4] = {0, 0, 0, 0};
  for (entity::ConfigId to : to_vec) {
    int to_row_id = to.row_id;
    int to_column_id = to.column_id;

    if (to_row_id < from_row_id) {
      output_vec[0] = 1;
    } else if (to_row_id > from_row_id) {
      output_vec[1] = 1;
    } else if (to_column_id < from_column_id) {
      output_vec[2] = 1;
    } else if (to_column_id > from_column_id) {
      output_vec[3] = 1;
    }
  }

  int result = 0;
  for (int i = 3; i >= 0; i--) {
    result *= 2;
    result += output_vec[i];
  }

  return result;
}

int main(int argc, char* argv[]) {
  std::string mapping_file_path =
      "./simulator/verilog/test/data/elastic_mapping_no_loop.json";

  std::shared_ptr<entity::Mapping> mapping_ptr =
      std::make_shared<entity::Mapping>();
  *mapping_ptr = io::ReadMappingFile(mapping_file_path);

  // verilog simulator
  auto GetInputPEIndex = [](entity::ConfigId from, entity::ConfigId to) {
    // std::cout << from.row_id << " " << from.column_id << "/" << to.row_id <<
    // " "
    //           << to.column_id << std::endl;
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
  Velastic_CGRA* cgra = new Velastic_CGRA();
  const int kCGRAContextCycle = 4;
  const int kNeighborPENum = 4;

  cgra->reset_n = 1;
  cgra->clk = 0;

  while (time_counter < 100) {
    if (time_counter == 1) {
      cgra->reset_n = 0;
    }
    cgra->eval();
    time_counter++;
  }

  Verilated::traceEverOn(true);
  VerilatedVcdC* tfp = new VerilatedVcdC;

  cgra->trace(tfp, 100);  // Trace 100 levels of hierarchy
  tfp->open("./output/cgra.vcd");

  cgra->reset_n = 1;
  cgra->clk = 1;
  int row_size = mapping_ptr->GetMRRGConfig().row;
  int column_size = mapping_ptr->GetMRRGConfig().column;
  int context_size = mapping_ptr->GetMRRGConfig().context_size;

  int cycle = 0;
  int config_size = row_size * column_size * context_size;
  entity::ConfigId output_config_id;
  std::vector<int> output_vec;
  while (time_counter < 1800) {
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
        if (cgra_config.from_config_id_num >= 1) {
          cgra->config_input_PE_index_1 =
              GetInputPEIndex(cgra_config.from_config_id_vec[0], config_id);
        } else {
          cgra->config_input_PE_index_1 = kNeighborPENum;
        }
        if (cgra_config.from_config_id_num == 2) {
          cgra->config_input_PE_index_2 =
              GetInputPEIndex(cgra_config.from_config_id_vec[1], config_id);
        } else {
          cgra->config_input_PE_index_2 = kNeighborPENum;
        }
        // std::cout << "from config num: " << cgra_config.from_config_id_num
        //           << std::endl;
        cgra->config_output_PE_index =
            GetOutputPEIndex(config_id, cgra_config.to_config_id_vec);
        // std::cout << ">>" << row_id << "," << column_id << "," << context_id
        //           << ":" << cgra->config_output_PE_index + 0 << ","
        //           << cgra->config_input_PE_index_1 + 0
        //           << cgra->config_input_PE_index_2 + 0 << std::endl;
        cgra->config_op = GetOpIndex(cgra_config.operation_type);
        cgra->config_const_data = cgra_config.const_value;
        if (cgra_config.operation_type != entity::OpType::NOP) {
          cgra->mapping_context_max_id = context_id;
        }

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
      } else if (config_size + 40 <= cycle && cycle <= config_size + 90) {
        cgra->memory_write = 0;
        cgra->start_exec = 0;
        int exe_cycle = cycle - (config_size + 40);

        std::cout << "-- step " << exe_cycle << " --" << std::endl;
        std::cout
            << "output/output_valid/input_stop/alu op/buffer input valid/mux "
               "valid/mux inpput "
               "index/join output "
               "stop/join output valid"
            << std::endl;
        for (int i = 0; i < 4; i++) {
          for (int j = 0; j < 4; j++) {
            std::cout
                << cgra->pe_output[i][j][0] << "/"
                << cgra->pe_valid_output[i][j][0] + 0
                << cgra->pe_valid_output[i][j][1] + 0
                << cgra->pe_valid_output[i][j][2] + 0
                << cgra->pe_valid_output[i][j][3] + 0 << "/"
                << cgra->pe_stop_input[i][j][0] + 0
                << cgra->pe_stop_input[i][j][1] + 0
                << cgra->pe_stop_input[i][j][2] + 0
                << cgra->pe_stop_input[i][j][3] + 0 << "/"
                << cgra->DEBUG_alu_op[i][j] + 0 << "/"
                << cgra->DEBUG_pe_buffer_input_valid[i][j] + 0 << "/"
                << cgra->DEBUG_pe_mux_output_valid[i][j][0] + 0
                << cgra->DEBUG_pe_mux_output_valid[i][j][1] + 0 << "/"
                << cgra->DEBUG_pe_mux_input_PE_index[i][j][0] + 0
                << cgra->DEBUG_pe_mux_input_PE_index[i][j][1] + 0
                << "/"
                // << cgra->DEBUG_pe_mux_output_stop[i][j][0] + 0
                // << cgra->DEBUG_pe_mux_output_stop[i][j][1] + 0 << "/"
                // << cgra->DEBUG_pe_fork_a_output_stop[i][j][0] + 0
                // << cgra->DEBUG_pe_fork_a_output_stop[i][j][1] + 0
                // << cgra->DEBUG_pe_fork_a_output_stop[i][j][2] + 0
                // << cgra->DEBUG_pe_fork_a_output_stop[i][j][3] + 0
                // << cgra->DEBUG_pe_fork_a_output_stop[i][j][4] + 0 << "/"
                // << cgra->DEBUG_pe_fork_a_output_valid[i][j][0] + 0
                // << cgra->DEBUG_pe_fork_a_output_valid[i][j][1] + 0
                // << cgra->DEBUG_pe_fork_a_output_valid[i][j][2] + 0
                // << cgra->DEBUG_pe_fork_a_output_valid[i][j][3] + 0
                // << cgra->DEBUG_pe_fork_a_output_valid[i][j][4] + 0 << "/"
                << cgra->DEBUG_pe_join_output_stop[i][j] + 0 << "/"
                << cgra->DEBUG_pe_join_output_valid[i][j] + 0 << " ";
          }
          std::cout << std::endl;
        }

        bool output_valid_output =
            cgra->pe_valid_output[output_config_id.row_id]
                                 [output_config_id.column_id][0];
        bool output_stop_output =
            cgra->pe_stop_input[output_config_id.row_id]
                               [output_config_id.column_id + 1][2];
        if (output_valid_output && !output_stop_output) {
          output_vec.push_back(cgra->pe_output[output_config_id.row_id]
                                              [output_config_id.column_id][0]);
        }
      }
    }

    cgra->eval();
    tfp->dump(time_counter);
    time_counter++;
  }

  cgra->final();
  tfp->close();
  for (int i : output_vec) {
    std::cout << "output_vec: " << i << std::endl;
  }
  int count = 0;
  std::cout << "output_vec_size: " << output_vec.size() << std::endl;
  //   for (int i = 0; i < 7; i++) {
  //     count += i * i * 2;
  //     std::cout << output_vec[i + 4] << ", " << count << std::endl;
  //   }
}