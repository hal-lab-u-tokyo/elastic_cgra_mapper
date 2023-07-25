`ifndef ELASTIC_ALU
`define ELASTIC_ALU

`include "param.v"

parameter BEFORE_EXEC = 2'b00;
parameter DURING_EXEC = 2'b01;
parameter FINISH_EXEC = 2'b10;

module ElasticALU (
    // base 
    input clk,
    input reset_n,
    // alu 
    input [DATA_WIDTH-1:0] input_data_1,
    input [DATA_WIDTH-1:0] input_data_2,
    input [OPERATION_BIT_LENGTH-1:0] op,
    input [DATA_WIDTH-1:0] const_data,
    output reg [DATA_WIDTH-1:0] output_data,
    // memory if
    output reg [ADDRESS_WIDTH-1:0] memory_write_address,
    output reg memory_write,
    output reg [DATA_WIDTH-1:0] memory_write_data,
    output reg [ADDRESS_WIDTH-1:0] memory_read_address,
    input [DATA_WIDTH-1:0] memory_read_data,
    // SELF protocol
    input valid_input,
    output stop_input,
    output valid_output,
    input stop_output,
    // config
    output switch_context
);
    wire output_transfer = valid_output & !(stop_output);
    wire input_transfer = (valid_input & !stop_input) | (op == 5) | (r_is_init & (op == 8)) ;
    reg [1:0] state;
    reg [DATA_WIDTH-1:0] op_cycle_counter;
    reg [DATA_WIDTH-1:0] r_input_data_1, r_input_data_2;
    reg r_is_init;
    wire [DATA_WIDTH-1:0] input_data_1_for_alu;
    assign input_data_1_for_alu = input_transfer ? input_data_1 : r_input_data_1;
    wire [DATA_WIDTH-1:0] input_data_2_for_alu = input_transfer ? input_data_2 : r_input_data_2;

    assign stop_input = (state >= DURING_EXEC);
    assign valid_output = (state == FINISH_EXEC);
    assign switch_context = output_transfer;

    function [DATA_WIDTH-1:0] getOpCycle(
        input [OPERATION_BIT_LENGTH-1:0] op);
        begin
            case (op)
                1: getOpCycle = ADD_CYCLE;
                2: getOpCycle = SUB_CYCLE;
                3: getOpCycle = MUL_CYCLE;
                4: getOpCycle = DIV_CYCLE;
                5: getOpCycle = CONST_CYCLE;
                6: getOpCycle = LOAD_CYCLE;
                7: getOpCycle = OUTPUT_CYCLE;
                8: getOpCycle = ROUTE_CYCLE;
                default: getOpCycle = 1;
            endcase
        end
    endfunction

    always_ff @(posedge clk, negedge reset_n) begin
        if (!reset_n) begin
            state <= BEFORE_EXEC;
            r_is_init <= 1;
            if (output_transfer & state == FINISH_EXEC) begin
                state <= BEFORE_EXEC;
            end else if (input_transfer | state == DURING_EXEC) begin
                r_is_init <= 0;
                if (input_transfer) begin
                    r_input_data_1 <= input_data_1;
                    r_input_data_2 <= input_data_2;
                end
                case (op)
                    0: output_data <= 0;  // nop
                    1: begin
                        output_data <= input_data_1_for_alu + input_data_2_for_alu;  // add 
                    end
                    2: begin
                        output_data <= input_data_1_for_alu - input_data_2_for_alu;  // sub
                    end
                    3: begin
                        output_data <= input_data_1_for_alu * input_data_2_for_alu;  // mul
                    end
                    4: begin
                        output_data <= input_data_1_for_alu / input_data_2_for_alu;  // div
                    end
                    5: begin
                        output_data <= const_data;  // const
                    end
                    6: begin  // load
                        memory_read_address <= input_data_1_for_alu[ADDRESS_WIDTH-1:0];
                        memory_write <= 1;
                        output_data <= memory_read_data;
                    end
                    7: begin
                        output_data <= input_data_1_for_alu;  // output 
                    end
                    8: begin
                        output_data <= input_data_1_for_alu;  // route
                    end
                endcase
                if (input_transfer) begin
                    op_cycle_counter <= getOpCycle(op) - 1;
                    if (getOpCycle(op) == 1) begin
                        state <= FINISH_EXEC;
                    end else begin
                        state <= DURING_EXEC;
                    end
                end
            end


            if (state == DURING_EXEC | op_cycle_counter > 0) begin
                op_cycle_counter <= op_cycle_counter - 1;
                if (op_cycle_counter == 1) begin
                    state <= FINISH_EXEC;
                end
            end
            // $display("--- ALU ---");
            // $display("state: ", state);
            // $display("input transfer: ", input_transfer);
            // $display("input valid: ", valid_input);
            // $display("input stop: ", stop_input);
            // $display("op_cycle_counter: ", op_cycle_counter);
            // $display("output_transfer: ", output_transfer);
            // $display("stop_output: ", stop_output);
            // $display("output data: ", output_data);
            // $display("output valid: ", valid_output);
            // $display("output stop: ", stop_output);
            // $display("input_data_1_for_alu: ", input_data_1_for_alu);
            // $display("input_data_2_for_alu: ", input_data_2_for_alu);
        end
    end

endmodule

`endif  // ELASTIC_ALU
