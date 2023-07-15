`ifndef ELASTIC_PE
`define ELASTIC_PE

`include "param.v"
`include "struct/elastic_wire.v"
`include "struct/elastic_config_data.v"
`include "elastic_module/elastic_buffer.v"
`include "elastic_module/elastic_fork.v"
`include "elastic_module/elastic_join.v"
`include "elastic_module/elastic_multiplexer.v"

module ElasticPE (
    input clk,
    input reset_n,
    // config load if
    input [NEIGHBOR_PE_NUM_BIT_LENGTH-1:0] config_input_PE_index_1,
    input [NEIGHBOR_PE_NUM_BIT_LENGTH-1:0] config_input_PE_index_2,
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
    output reg [ADDRESS_WIDTH-1:0] memory_write_address,
    output reg memory_write,
    output reg [DATA_WIDTH-1:0] memory_write_data,
    output reg [ADDRESS_WIDTH-1:0] memory_read_address,
    input [DATA_WIDTH-1:0] memory_read_data,
    // SELF protocol
    input valid_input[NEIGHBOR_PE_NUM],
    output stop_input[NEIGHBOR_PE_NUM],
    output valid_output[NEIGHBOR_PE_NUM],
    input stop_output[NEIGHBOR_PE_NUM]
);
    ElasticConfigData r_config_memory[CONTEXT_SIZE];

    reg [CONTEXT_SIZE_BIT_LENGTH-1:0] r_config_index;
    reg [DATA_WIDTH-1:0] op_cycle_counter;

    // Fork -> Mux
    wire [DATA_WIDTH-1:0]        w_fork_a_output_data[NEIGHBOR_PE_NUM], w_fork_b_output_data[NEIGHBOR_PE_NUM];
    wire w_fork_a_output_valid[NEIGHBOR_PE_NUM], w_fork_b_output_valid[NEIGHBOR_PE_NUM];
    wire w_fork_a_output_stop[NEIGHBOR_PE_NUM], w_fork_b_output_stop[NEIGHBOR_PE_NUM];

    // Mux -> Join
    ElasticWire w_mux_a_output, w_mux_b_output;

    // Join -> ALU
    wire [DATA_WIDTH-1:0] w_elastic_join_data_output[2];
    wire w_elastic_join_valid_output;
    reg w_elastic_join_stop_output;

    // ALU -> Buffer
    ElasticWire r_alu_output;
    genvar i;
    for (i = 0; i < NEIGHBOR_PE_NUM; i++) begin
        assign pe_output_data[i] = r_alu_output.data;
    end

    // Buffer -> Fork
    ElasticWire w_buffer_output;

    // Elastic Module
    // Elastic Module : Fork
    wire [NEIGHBOR_PE_NUM-1:0] available_output;
    assign available_output = ~0;
    generate
        for (i = 0; i < NEIGHBOR_PE_NUM; i++) begin : GenerateFork
            wire [DATA_WIDTH-1:0] fork_output_data[NEIGHBOR_PE_NUM];
            wire fork_valid_output[NEIGHBOR_PE_NUM];
            wire fork_stop_output[NEIGHBOR_PE_NUM];

            assign fork_output_data[0]  = w_fork_a_output_data[i];
            assign fork_output_data[1]  = w_fork_b_output_data[i];
            assign fork_valid_output[0] = w_fork_a_output_valid[i];
            assign fork_valid_output[1] = w_fork_b_output_valid[i];
            assign fork_stop_output[0]  = w_fork_a_output_stop[i];
            assign fork_stop_output[1]  = w_fork_b_output_stop[i];


            ElasticFork elastic_fork (
                .clk(clk),
                .reset_n(reset_n),
                .input_data(pe_input_data[i]),
                .valid_input(valid_input[i]),
                .stop_input(stop_input[i]),
                .output_data(fork_output_data),
                .valid_output(fork_valid_output),
                .stop_output(stop_output),
                .available_output(available_output)
            );
        end
    endgenerate

    // Elastic Module: Mux
    ElasticMultiplexer elastic_mux_a (
        .data_input(w_fork_a_output_data),
        .valid_input(w_fork_a_output_valid),
        .stop_input(w_fork_a_output_stop),
        .data_output(w_mux_a_output.data),
        .valid_output(w_mux_a_output.valid),
        .stop_output(w_mux_a_output.stop),
        .input_data_index(r_config_memory[r_config_index].input_PE_index_1)
    );
    ElasticMultiplexer elastic_mux_b (
        .data_input(w_fork_b_output_data),
        .valid_input(w_fork_b_output_valid),
        .stop_input(w_fork_b_output_stop),
        .data_output(w_mux_b_output.data),
        .valid_output(w_mux_b_output.valid),
        .stop_output(w_mux_b_output.stop),
        .input_data_index(r_config_memory[r_config_index].input_PE_index_2)
    );

    // Elastic Module : Join
    wire [DATA_WIDTH-1:0] w_elastic_join_data_input[2];
    wire w_elastic_join_valid_input[2], w_elastic_join_stop_input[2];

    assign w_elastic_join_data_input[0]  = w_mux_a_output.data;
    assign w_elastic_join_data_input[1]  = w_mux_b_output.data;
    assign w_elastic_join_valid_input[0] = w_mux_a_output.valid;
    assign w_elastic_join_valid_input[1] = w_mux_b_output.valid;
    assign w_elastic_join_stop_input[0]  = w_mux_a_output.stop;
    assign w_elastic_join_stop_input[1]  = w_mux_b_output.stop;

    ElasticJoin elastic_join (
        .data_input  (w_elastic_join_data_input),
        .valid_input (w_elastic_join_valid_input),
        .stop_input  (w_elastic_join_stop_input),
        .data_output (w_elastic_join_data_output),
        .valid_output(w_elastic_join_valid_output),
        .stop_output (w_elastic_join_stop_output)
    );

    // Elastic Module : ALU
    wire w_alu_input_transfer = w_elastic_join_valid_output &!w_elastic_join_stop_output;
    wire w_alu_output_transfer = r_alu_output.valid & !r_alu_output.stop;

    // Elastic Module : Buffer
    ElasticWire w_alu_output;
    assign w_alu_output.data  = r_alu_output.data;
    assign w_alu_output.valid = r_alu_output.valid;
    assign w_alu_output.stop  = r_alu_output.stop;
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
        .available_output(r_config_memory[r_config_index].output_PE_index)
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
            end else if (w_alu_input_transfer | op_cycle_counter > 0) begin
                // operation
                case (r_config_memory[r_config_index].op)
                    0: r_alu_output.data <= 0;  // nop
                    1: begin
                        r_alu_output.data <= pe_input_data[r_config_memory[r_config_index].input_PE_index_1] + pe_input_data[r_config_memory[r_config_index].input_PE_index_2];  // add 
                        op_cycle_counter <= ADD_CYCLE;
                    end
                    2: begin
                        r_alu_output.data <= pe_input_data[r_config_memory[r_config_index].input_PE_index_1] - pe_input_data[r_config_memory[r_config_index].input_PE_index_2];  // sub
                        op_cycle_counter <= SUB_CYCLE;
                    end
                    3: begin
                        r_alu_output.data <= pe_input_data[r_config_memory[r_config_index].input_PE_index_1] * pe_input_data[r_config_memory[r_config_index].input_PE_index_2];  // mul
                        op_cycle_counter <= MUL_CYCLE;
                    end
                    4: begin
                        r_alu_output.data <= pe_input_data[r_config_memory[r_config_index].input_PE_index_1] / pe_input_data[r_config_memory[r_config_index].input_PE_index_2];  //div
                        op_cycle_counter <= DIV_CYCLE;
                    end
                    5: begin
                        r_alu_output.data <= r_config_memory[r_config_index].const_data;  //const
                        op_cycle_counter <= CONST_CYCLE;
                    end
                    6: begin  //load
                        memory_read_address <= pe_input_data[r_config_memory[r_config_index].input_PE_index_1][ADDRESS_WIDTH-1:0];
                        memory_write <= 1;
                        r_alu_output.data <= memory_read_data;
                        op_cycle_counter <= LOAD_CYCLE;
                    end
                    7: begin
                        r_alu_output.data <= pe_input_data[r_config_memory[r_config_index].input_PE_index_1];  //output 
                        op_cycle_counter <= OUTPUT_CYCLE;
                    end
                    8: begin
                        r_alu_output.data <= pe_input_data[r_config_memory[r_config_index].input_PE_index_1];  // route
                        op_cycle_counter <= ROUTE_CYCLE;
                    end
                endcase
                if (op_cycle_counter > 0) begin  // during op
                    op_cycle_counter <= op_cycle_counter - 1;
                    if (op_cycle_counter == 1) begin
                        r_alu_output.valid <= 1;
                        w_elastic_join_stop_output <= 0;
                    end
                end else begin  // beginning of op
                    r_alu_output.valid <= 0;
                    w_elastic_join_stop_output <= 1;
                end
            end

            // context reset 
            if (start_exec) begin
                r_config_index <= 0;
            end

            // context switch
            if (w_alu_output_transfer) begin
                // context update
                if (r_config_index == mapping_context_max_id) begin
                    r_config_index <= 0;
                end else begin
                    r_config_index <= r_config_index + 1;
                end
            end
        end

        // debug
    end
endmodule

`endif  // SYNCHRONOUSE_PE
