`ifndef ELASTIC_MULTIPLEXER
`define ELASTIC_MULTIPLEXER
`include "param.v"

module ElasticMultiplexer (
    // input
    input [DATA_WIDTH-1:0] data_input[NEIGHBOR_PE_NUM],
    input valid_input[NEIGHBOR_PE_NUM],
    output stop_input[NEIGHBOR_PE_NUM],
    // output
    output [DATA_WIDTH-1:0] data_output,
    output valid_output,
    input stop_output,
    // multiplexer
    input [NEIGHBOR_PE_NUM_BIT_LENGTH-1:0] input_data_index
);
    /* verilator lint_off UNOPTFLAT */
    wire valid_output_immediate[NEIGHBOR_PE_NUM + 1];

    assign valid_output_immediate[0] = 1;
    assign data_output = data_input[input_data_index];
    genvar i;
    for (i = 0; i < NEIGHBOR_PE_NUM; i++) begin
        assign stop_input[i] = stop_output;
        assign valid_output_immediate[i+1]  = valid_output_immediate[i] & valid_input[i];
    end
    assign valid_output = valid_output_immediate[NEIGHBOR_PE_NUM];
endmodule

`endif  // ELASTIC_MULTIPLEXER
