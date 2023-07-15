`ifndef ELASTIC_FORK
`define ELASTIC_FORK

`include "param.v"

module ElasticFork (
    // base
    input clk,
    input reset_n,
    // input 
    input [DATA_WIDTH-1:0] input_data,
    input valid_input,
    output stop_input,
    // output
    output [DATA_WIDTH-1:0] output_data[NEIGHBOR_PE_NUM],
    output valid_output[NEIGHBOR_PE_NUM],
    input stop_output[NEIGHBOR_PE_NUM],
    // connection
    input [NEIGHBOR_PE_NUM-1:0] available_output
);
    reg prev_reg[NEIGHBOR_PE_NUM];
    wire input_retry = valid_input & stop_input;
    /* verilator lint_off UNOPTFLAT */
    wire stop_input_intermediate[NEIGHBOR_PE_NUM + 1];
    assign stop_input_intermediate[0] = 0;
    wire available_stop_output[NEIGHBOR_PE_NUM];

    genvar i;
    for (i = 0; i < NEIGHBOR_PE_NUM; i++) begin
        assign output_data[i] = input_data;
        wire available_valid_input = valid_input && available_output[i];
        assign valid_output[i] = prev_reg[i] & available_valid_input;

        assign available_stop_output[i] = stop_output[i] & available_output[i];
        assign stop_input_intermediate[i+1] = stop_input_intermediate[i] | (available_stop_output[i] & prev_reg[i]);
    end
    assign stop_input = stop_input_intermediate[NEIGHBOR_PE_NUM];

    always_ff @(posedge clk, negedge reset_n) begin
        begin
            for (int i = 0; i < NEIGHBOR_PE_NUM; i++) begin
                prev_reg[i] <= !input_retry | (prev_reg[i] & available_stop_output[i]);
            end
        end
    end
endmodule

`endif  // ELASTIC_FORK
