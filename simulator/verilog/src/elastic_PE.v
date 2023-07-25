`ifndef ELASTIC_PE
`define ELASTIC_PE

`include "param.v"
`include "struct/elastic_wire.v"
`include "struct/elastic_config_data.v"
`include "elastic_module/elastic_buffer.v"
`include "elastic_module/elastic_fork.v"
`include "elastic_module/elastic_join.v"
`include "elastic_module/elastic_multiplexer.v"
`include "elastic_module/elastic_alu.v"

module ElasticPE (
    input clk,
    input reset_n,
    // config load if
    input [INPUT_NUM_BIT_LENGTH-1:0] config_input_PE_index_1,
    input [INPUT_NUM_BIT_LENGTH-1:0] config_input_PE_index_2,
    input [NEIGHBOR_PE_NUM-1:0] config_output_PE_index,
    input [OPERATION_BIT_LENGTH-1:0] config_op,
    input [DATA_WIDTH-1:0] config_const_data,
    input write_config_data,
    input [CONTEXT_SIZE_BIT_LENGTH-1:0] config_index,
    // execution param
    input start_exec,
    input [CONTEXT_SIZE_BIT_LENGTH-1:0] mapping_context_max_id,
    // PE if
    input [DATA_WIDTH-1:0] pe_input_data[NEIGHBOR_PE_NUM],
    output [DATA_WIDTH-1:0] pe_output_data[NEIGHBOR_PE_NUM],
    // memory if
    output [ADDRESS_WIDTH-1:0] memory_write_address,
    output memory_write,
    output [DATA_WIDTH-1:0] memory_write_data,
    output [ADDRESS_WIDTH-1:0] memory_read_address,
    input [DATA_WIDTH-1:0] memory_read_data,
    // SELF protocol
    input valid_input[NEIGHBOR_PE_NUM],
    output stop_input[NEIGHBOR_PE_NUM],
    output valid_output[NEIGHBOR_PE_NUM],
    input stop_output[NEIGHBOR_PE_NUM],
    // DEBUG
    output [CONTEXT_SIZE_BIT_LENGTH-1:0] DEBUG_alu_context_id
);
    ElasticConfigData r_config_memory[CONTEXT_SIZE];

    reg [CONTEXT_SIZE_BIT_LENGTH-1:0] r_config_index_mux_a;
    reg [CONTEXT_SIZE_BIT_LENGTH-1:0] r_config_index_mux_b;
    reg [CONTEXT_SIZE_BIT_LENGTH-1:0] r_config_index_alu;
    reg [CONTEXT_SIZE_BIT_LENGTH-1:0] r_config_index_fork;
    reg [DATA_WIDTH-1:0] op_cycle_counter;

    assign DEBUG_alu_context_id = r_config_index_alu;

    // Fork -> Mux
    wire [DATA_WIDTH-1:0]        w_fork_a_output_data[INPUT_NUM], w_fork_b_output_data[INPUT_NUM];
    wire w_fork_a_output_valid[INPUT_NUM], w_fork_b_output_valid[INPUT_NUM];
    wire w_fork_a_output_stop[INPUT_NUM], w_fork_b_output_stop[INPUT_NUM];

    // Mux -> Join
    ElasticWire w_mux_a_output, w_mux_b_output;
    wire w_switch_context_mux_a, w_switch_context_mux_b;
    wire [DATA_WIDTH-1:0] w_elastic_join_data_input[2];
    wire w_elastic_join_valid_input[2], w_elastic_join_stop_input[2];

    // Join -> ALU
    wire [DATA_WIDTH-1:0] w_elastic_join_data_output[2];
    wire w_elastic_join_valid_output, w_elastic_join_stop_output;

    // ALU -> Buffer
    ElasticWire w_alu_output;
    wire w_switch_context_alu;
    reg [DATA_WIDTH-1:0] r_alu_output[PE_REG_SIZE];
    reg [PE_REG_SIZE_BIT_LENGTH-1:0] r_alu_output_index;

    // Buffer -> Fork
    ElasticWire w_buffer_output;
    wire w_switch_context_fork;

    // Elastic Module
    // Elastic Module : Fork
    wire [NEIGHBOR_PE_NUM-1:0] available_output;
    assign available_output = ~0;
    genvar i, j;
    generate
        for (i = 0; i < NEIGHBOR_PE_NUM; i++) begin : GenerateFork
            wire [DATA_WIDTH-1:0] fork_output_data[NEIGHBOR_PE_NUM];
            wire fork_valid_output[NEIGHBOR_PE_NUM];
            wire fork_stop_output[NEIGHBOR_PE_NUM];
            wire dummy_context_switch_fork;

            assign w_fork_a_output_data[i] = fork_output_data[0];
            assign w_fork_b_output_data[i] = fork_output_data[1];
            assign w_fork_a_output_valid[i] = fork_valid_output[0];
            assign w_fork_b_output_valid[i] = fork_valid_output[1];
            assign fork_stop_output[0] = w_fork_a_output_stop[i];
            assign fork_stop_output[1] = w_fork_b_output_stop[i];

            for (j = 2; j < NEIGHBOR_PE_NUM; j++) begin : GenerateForkOutputWire
                assign fork_stop_output[j] = 0;
            end

            ElasticFork elastic_fork (
                .clk(clk),
                .reset_n(reset_n),
                .input_data(pe_input_data[i]),
                .valid_input(valid_input[i]),
                .stop_input(stop_input[i]),
                .output_data(fork_output_data),
                .valid_output(fork_valid_output),
                .stop_output(fork_stop_output),
                .available_output(available_output),
                .switch_context(dummy_context_switch_fork)
            );
        end
    endgenerate
    generate
        for (i = 0; i < PE_REG_SIZE; i++) begin : AddRegValueToForkOutput
            assign w_fork_a_output_data[NEIGHBOR_PE_NUM+i]  = r_alu_output[i];
            assign w_fork_b_output_data[NEIGHBOR_PE_NUM+i]  = r_alu_output[i];
            assign w_fork_a_output_valid[NEIGHBOR_PE_NUM+i] = 1;
            assign w_fork_b_output_valid[NEIGHBOR_PE_NUM+i] = 1;
        end
    endgenerate


    // Elastic Module: Mux
    ElasticMultiplexer elastic_mux_a (
        .data_input(w_fork_a_output_data),
        .valid_input(w_fork_a_output_valid),
        .stop_input(w_fork_a_output_stop),
        .data_output(w_mux_a_output.data),
        .valid_output(w_mux_a_output.valid),
        .stop_output(w_elastic_join_stop_input[0]),
        .input_data_index(r_config_memory[r_config_index_mux_a].input_PE_index_1),
        .switch_context(w_switch_context_mux_a)
    );
    ElasticMultiplexer elastic_mux_b (
        .data_input(w_fork_b_output_data),
        .valid_input(w_fork_b_output_valid),
        .stop_input(w_fork_b_output_stop),
        .data_output(w_mux_b_output.data),
        .valid_output(w_mux_b_output.valid),
        .stop_output(w_elastic_join_stop_input[1]),
        .input_data_index(r_config_memory[r_config_index_mux_b].input_PE_index_2),
        .switch_context(w_switch_context_mux_b)
    );

    // Elastic Module : Join
    assign w_elastic_join_data_input[0]  = w_mux_a_output.data;
    assign w_elastic_join_data_input[1]  = w_mux_b_output.data;
    assign w_elastic_join_valid_input[0] = w_mux_a_output.valid;
    assign w_elastic_join_valid_input[1] = w_mux_b_output.valid;

    ElasticJoin elastic_join (
        .data_input  (w_elastic_join_data_input),
        .valid_input (w_elastic_join_valid_input),
        .stop_input  (w_elastic_join_stop_input),
        .data_output (w_elastic_join_data_output),
        .valid_output(w_elastic_join_valid_output),
        .stop_output (w_elastic_join_stop_output)
    );

    // Elastic Module : ALU
    ElasticALU elastic_alu (
        .clk(clk),
        .reset_n(reset_n),
        .input_data_1(w_elastic_join_data_output[0]),
        .input_data_2(w_elastic_join_data_output[1]),
        .op(r_config_memory[r_config_index_alu].op),
        .const_data(r_config_memory[r_config_index_alu].const_data),
        .output_data(w_alu_output.data),
        .memory_write_address(memory_write_address),
        .memory_write(memory_write),
        .memory_write_data(memory_write_data),
        .memory_read_address(memory_read_address),
        .memory_read_data(memory_read_data),
        .valid_input(w_elastic_join_valid_output),
        .stop_input(w_elastic_join_stop_output),
        .valid_output(w_alu_output.valid),
        .stop_output(w_alu_output.stop),
        .switch_context(w_switch_context_alu)
    );

    // Elastic Module : Buffer
    wire [ELASTIC_BUFFER_SIZE_BIT_LENGTH:0] w_buffer_data_size;
    ElasticBuffer elastic_buffer (
        .clk(clk),
        .reset_n(reset_n),
        .valid_input(w_alu_output.valid),
        .stop_input(w_alu_output.stop),
        .data_input(w_alu_output.data),
        .valid_output(w_buffer_output.valid),
        .stop_output(w_buffer_output.stop),
        .data_output(w_buffer_output.data),
        .DEBUG_data_size(w_buffer_data_size)
    );

    // Elastic Module : Fork
    ElasticFork elastic_fork (
        .clk(clk),
        .reset_n(reset_n),
        .input_data(w_buffer_output.data),
        .valid_input(w_buffer_output.valid),
        .stop_input(w_buffer_output.stop),
        .output_data(pe_output_data),
        .valid_output(valid_output),
        .stop_output(stop_output),
        .available_output(r_config_memory[r_config_index_fork].output_PE_index),
        .switch_context(w_switch_context_fork)
    );

    // data for operation execution
    always_ff @(posedge clk, negedge reset_n) begin
        if (!reset_n) begin
            // reset config memory
            integer i;
            for (i = 0; i < CONTEXT_SIZE; ++i) begin
                r_config_memory[i] <= 0;
            end
        end else begin
            if (write_config_data) begin
                // write config memory
                r_config_memory[config_index].input_PE_index_1 <= config_input_PE_index_1;
                r_config_memory[config_index].input_PE_index_2 <= config_input_PE_index_2;
                r_config_memory[config_index].output_PE_index<=config_output_PE_index;
                r_config_memory[config_index].op <= config_op;
                r_config_memory[config_index].const_data <= config_const_data;
            end

            // context reset 
            if (start_exec) begin
                r_config_index_mux_a <= 0;
                r_config_index_mux_b <= 0;
                r_config_index_alu   <= 0;
                r_config_index_fork  <= 0;
            end

            // context switch : mux
            if (w_switch_context_mux_a) begin
                if (r_config_index_mux_a == mapping_context_max_id) begin
                    r_config_index_mux_a <= 0;
                end else begin
                    r_config_index_mux_a <= r_config_index_mux_a + 1;
                end
            end
            if (w_switch_context_mux_b) begin
                if (r_config_index_mux_b == mapping_context_max_id) begin
                    r_config_index_mux_b <= 0;
                end else begin
                    r_config_index_mux_b <= r_config_index_mux_b + 1;
                end
            end
            // context switch & register update : alu
            if (w_switch_context_alu) begin
                // register update       
                r_alu_output[r_alu_output_index] <= w_alu_output.data;
                if (r_alu_output_index == PE_REG_SIZE - 1) begin
                    r_alu_output_index <= 0;
                end else begin
                    r_alu_output_index <= r_alu_output_index + 1;
                end

                // alu config update
                if (r_config_index_alu == mapping_context_max_id) begin
                    r_config_index_alu <= 0;
                end else begin
                    r_config_index_alu <= r_config_index_alu + 1;
                end
            end
            // context switch : fork
            if (w_switch_context_fork) begin
                if (r_config_index_fork == mapping_context_max_id) begin
                    r_config_index_fork <= 0;
                end else begin
                    r_config_index_fork <= r_config_index_fork + 1;
                end
            end

            // $display("--- PE ---");
            // $display("r_config_index_alu: ", r_config_index_alu);
            // $display("input PE index 1: ",
            //          r_config_memory[r_config_index].input_PE_index_1);
            // $display("input PE index 2: ",
            //          r_config_memory[r_config_index].input_PE_index_2);
            // $display("input PE data 1: ", pe_input_data[0]);
            // $display("input PE data 2: ", pe_input_data[1]);

            // $display("--- join ---");
            // $display("elastic join data [0]: ", w_elastic_join_data_output[0]);
            // $display("elastic join data [1]: ", w_elastic_join_data_output[1]);
            // $display("elastic join input valid[0]: ",
            //          w_elastic_join_valid_input[0]);
            // $display("elastic join input stop[0]: ",
            //          w_elastic_join_stop_input[0]);
            // $display("elastic join output valid: ",
            //          w_elastic_join_valid_output);
            // $display("elastic join output stop: ", w_elastic_join_stop_output);
            // $display("--- mux ---");
            // $display("elastic mux a output valid: ", w_mux_a_output.valid);
            // $display("elastic mux a output stop: ", w_mux_a_output.stop);
            // $display("elastic mux a output data: ", w_mux_a_output.data);
            // $display("elastic mux b output valid: ", w_mux_b_output.valid);
            // $display("elastic mux b output stop: ", w_mux_b_output.stop);
            // $display("elastic mux b output data: ", w_mux_b_output.data);
            // $display("mux b index: ", r_config_index_mux_b);
            // $display("--- fork ---");
            // $display("fork index: ", r_config_index_fork);
            // $display("fork output valid from reg: ", w_fork_b_output_valid[4]);
        end

        // debug
    end
endmodule

`endif  // SYNCHRONOUSE_PE
