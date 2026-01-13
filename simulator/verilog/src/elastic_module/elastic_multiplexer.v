`ifndef ELASTIC_MULTIPLEXER
`define ELASTIC_MULTIPLEXER
`include "param.v"

module ElasticMultiplexer (
    // input
    input [DATA_WIDTH-1:0] data_input[INPUT_NUM],
    input valid_input[INPUT_NUM],
    output stop_input[INPUT_NUM],
    // output
    output [DATA_WIDTH-1:0] data_output,
    output valid_output,
    input stop_output,
    // multiplexer
    input [INPUT_NUM_BIT_LENGTH-1:0] input_data_index,
    output switch_context
);
    assign data_output = data_input[input_data_index];
    assign valid_output = valid_input[input_data_index];
    assign switch_context = valid_output & !stop_output;

    genvar i;
    for (i = 0; i < INPUT_NUM; i++) begin
        assign stop_input[i] = stop_output;
    end
endmodule

`endif  // ELASTIC_MULTIPLEXER
