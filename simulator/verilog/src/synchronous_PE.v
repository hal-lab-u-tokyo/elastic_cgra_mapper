`ifndef SYNCHRONOUS_PE
`define SYNCHRONOUS_PE

`include "param.v"
`include "struct/config_data.v"

module PE (
    input clk,
    input reset_n,
    // config load if
    input [NEIGHBOR_PE_NUM_BIT_LENGTH-1:0] config_input_PE_index_1,
    input [NEIGHBOR_PE_NUM_BIT_LENGTH-1:0] config_input_PE_index_2,
    input [OPERATION_BIT_LENGTH-1:0] config_op,
    input [DATA_WIDTH-1:0] config_const_data,
    input write_config_data,
    input [CONTEXT_SIZE_BIT_LENGTH-1:0] config_index,
    // execution param
    input start_exec,
    input [CONTEXT_SIZE_BIT_LENGTH-1:0] mapping_context_max_id,
    // PE if
    input [DATA_WIDTH-1:0] pe_input_data[NEIGHBOR_PE_NUM],
    output reg [DATA_WIDTH-1:0] pe_output_data,
    // memory if
    output reg [ADDRESS_WIDTH-1:0] memory_write_address,
    output reg memory_write,
    output reg [DATA_WIDTH-1:0] memory_write_data,
    output reg [ADDRESS_WIDTH-1:0] memory_read_address,
    input [DATA_WIDTH-1:0] memory_read_data,
    // DEBUG
    output [DATA_WIDTH-1:0] DEBUG_input_PE_data_1,
    output [DATA_WIDTH-1:0] DEBUG_input_PE_data_2,
    output [NEIGHBOR_PE_NUM_BIT_LENGTH-1:0] DEBUG_input_PE_index_1,
    output [NEIGHBOR_PE_NUM_BIT_LENGTH-1:0] DEBUG_input_PE_index_2
);
    ConfigData r_config_memory[CONTEXT_SIZE];

    reg [CONTEXT_SIZE_BIT_LENGTH-1:0] r_config_index;
    reg [DATA_WIDTH-1:0] r_output;
    reg [1:0] r_counter;

    assign DEBUG_input_PE_data_1 = pe_input_data[r_config_memory[r_config_index].input_PE_index_1];
    assign DEBUG_input_PE_data_2 = pe_input_data[r_config_memory[r_config_index].input_PE_index_2];
    assign DEBUG_input_PE_index_1 = r_config_memory[r_config_index].input_PE_index_1;
    assign DEBUG_input_PE_index_2 = r_config_memory[r_config_index].input_PE_index_2;
    // data for operation execution
    always_ff @(posedge clk, negedge reset_n) begin
        if (!reset_n) begin
            // reset config memory
            integer i;
            for (i = 0; i < CONTEXT_SIZE; ++i) begin
                r_config_memory[i] <= 0;
            end
            r_counter <= 0;
        end else begin
            if (write_config_data) begin
                // write config memory
                r_config_memory[config_index].input_PE_index_1 <= config_input_PE_index_1;
                r_config_memory[config_index].input_PE_index_2 <= config_input_PE_index_2;
                r_config_memory[config_index].op <= config_op;
                r_config_memory[config_index].const_data <= config_const_data;
            end else begin
                r_counter <= r_counter + 1;
                // operation
                case (r_config_memory[r_config_index].op)
                    0: r_output <= 0;  // nop
                    1: begin
                        r_output <= pe_input_data[r_config_memory[r_config_index].input_PE_index_1] + pe_input_data[r_config_memory[r_config_index].input_PE_index_2];  // add 
                    end
                    2: begin
                        r_output <= pe_input_data[r_config_memory[r_config_index].input_PE_index_1] - pe_input_data[r_config_memory[r_config_index].input_PE_index_2];  // sub
                    end
                    3: begin
                        r_output <= pe_input_data[r_config_memory[r_config_index].input_PE_index_1] * pe_input_data[r_config_memory[r_config_index].input_PE_index_2];  // mul
                    end
                    4: begin
                        r_output <= pe_input_data[r_config_memory[r_config_index].input_PE_index_1] / pe_input_data[r_config_memory[r_config_index].input_PE_index_2];  //div
                    end
                    5: begin
                        r_output <= r_config_memory[r_config_index].const_data;  //const
                    end
                    6: begin  //load
                        memory_read_address <= pe_input_data[r_config_memory[r_config_index].input_PE_index_1][ADDRESS_WIDTH-1:0];
                        memory_write <= 1;
                        r_output <= memory_read_data;
                    end
                    7: begin
                        r_output <= pe_input_data[r_config_memory[r_config_index].input_PE_index_1];  //output 
                    end
                    8: begin
                        r_output <= pe_input_data[r_config_memory[r_config_index].input_PE_index_1];  // route
                    end
                endcase
            end

            // context reset 
            if (start_exec) begin
                r_config_index <= 0;
                r_counter <= 0;
            end

            // context switch
            if (r_counter == CONTEXT_SWITCH_CLK_SIZE) begin
                // context update
                if (r_config_index == mapping_context_max_id) begin
                    r_config_index <= 0;
                end else begin
                    r_config_index <= r_config_index + 1;
                end

                pe_output_data <= r_output;
                r_counter <= 0;
            end
        end

        // debug
    end
endmodule

`endif  // SYNCHRONOUSE_PE
