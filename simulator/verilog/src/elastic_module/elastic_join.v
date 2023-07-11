`ifndef ELASTIC_JOIN
`define ELASTIC_JOIN

`include "param.v"

module ElasticJoin (
    // input
    input [DATA_WIDTH-1:0] data_input[2],
    input valid_input[2],
    output stop_input[2],
    // output
    output [DATA_WIDTH-1:0] data_output[2],
    output valid_output,
    input stop_output
);
    assign data_output  = data_input;
    assign valid_output = valid_input[0] & valid_input[1];
    wire stop_input_data = !(!stop_output & valid_output);
    assign stop_input[0] = stop_input_data;
    assign stop_input[1] = stop_input_data;
endmodule

`endif  // ELASTIC_JOIN
