#include <Vsynchronous_PE.h>
#include <gtest/gtest.h>
#include <verilated.h>

#include <verilog_runner/packed_signals.hpp>

namespace packed = simulator::signals;

TEST(VerilogSimulatorTest, synchronous_PE_test) {
  Vsynchronous_PE pe;
  auto Clock = [&pe]() {
    pe.clk = 0;
    pe.eval();
    pe.clk = 1;
    pe.eval();
  };

  pe.reset_n = 0;
  pe.clk = 0;
  pe.eval();
  Clock();
  pe.reset_n = 1;
  pe.mapping_context_max_id = 7;

  for (int context = 0; context < 8; ++context) {
    pe.write_config_data = 1;
    pe.config_index = context;
    packed::SetBits(pe.config_data, 39, 3, context % 4);
    packed::SetBits(pe.config_data, 36, 3, (context + 1) % 4);
    packed::SetBits(pe.config_data, 0, 32, (context + 1) * 2);
    packed::SetBits(pe.config_data, 32, 4, context + 1);
    Clock();
  }

  pe.write_config_data = 0;
  pe.start_exec = 1;
  Clock();
  pe.start_exec = 0;

  auto ExecuteContext = [&pe, &Clock](int context) {
    ASSERT_EQ(pe.DEBUG_alu_context_id, context);
    Clock();
  };

  pe.pe_input_data[0] = 12;
  pe.pe_input_data[1] = 13;
  ExecuteContext(0);
  EXPECT_EQ(pe.pe_output_data, 25);

  pe.pe_input_data[1] = 20;
  pe.pe_input_data[2] = 15;
  ExecuteContext(1);
  EXPECT_EQ(pe.pe_output_data, 5);

  pe.pe_input_data[2] = 4;
  pe.pe_input_data[3] = 15;
  ExecuteContext(2);
  EXPECT_EQ(pe.pe_output_data, 60);

  pe.pe_input_data[3] = 62;
  pe.pe_input_data[0] = 2;
  ExecuteContext(3);
  EXPECT_EQ(pe.pe_output_data, 31);

  ExecuteContext(4);
  EXPECT_EQ(pe.pe_output_data, 10);

  pe.pe_input_data[1] = 20;
  ExecuteContext(5);
  EXPECT_EQ(pe.memory_read_address, 20);

  pe.pe_input_data[2] = 15;
  ExecuteContext(6);
  EXPECT_EQ(pe.pe_output_data, 15);

  pe.pe_input_data[3] = 20;
  ExecuteContext(7);
  EXPECT_EQ(pe.pe_output_data, 20);
}
