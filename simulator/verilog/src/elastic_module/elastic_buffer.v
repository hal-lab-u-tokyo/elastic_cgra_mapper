`ifndef ELASTIC_BUFFER
`define ELASTIC_BUFFER

`include "param.v"

// Reference
// Cortadella, Jordi, Mike Kishinevsky, and Bill Grundmann. "Synthesis of synchronous elastic architectures." Proceedings of the 43rd Annual Design Automation Conference. 2006.

module ElasticBuffer (
    // base signal
    input clk,
    input reset_n,
    // from input env
    input valid_input,
    output stop_input,
    input [DATA_WIDTH-1:0] data_input,
    // to output env
    output valid_output,
    input stop_output,
    output [DATA_WIDTH-1:0] data_output,
    // DEBUG
    // output [DATA_WIDTH-1:0] DEBUG_data_array[ELASTIC_BUFFER_SIZE],
    // output [ELASTIC_BUFFER_SIZE_BIT_LENGTH-1:0] DEBUG_read_index,
    // output [ELASTIC_BUFFER_SIZE_BIT_LENGTH-1:0] DEBUG_write_index,
    output [ELASTIC_BUFFER_SIZE_BIT_LENGTH:0] DEBUG_data_size
);
    reg [DATA_WIDTH-1:0] data_array[ELASTIC_BUFFER_SIZE];
    reg [ELASTIC_BUFFER_SIZE_BIT_LENGTH-1:0] read_index, write_index;
    reg [ELASTIC_BUFFER_SIZE_BIT_LENGTH:0] data_size;

    assign data_output  = data_array[read_index];
    assign valid_output = data_size != 0;
    assign stop_input   = data_size === ELASTIC_BUFFER_SIZE;

    // assign DEBUG_data_array = data_array;
    // assign DEBUG_read_index = read_index;
    // assign DEBUG_write_index = write_index;
    assign DEBUG_data_size = data_size;

    always_ff @(posedge clk, negedge reset_n) begin
        if (!reset_n) begin
            read_index  <= 0;
            write_index <= 0;
            data_size   <= 0;
        end else begin
            // Sequantial behavior
            if (valid_output && !stop_output) begin
                read_index <= read_index + 1;
                data_size  <= data_size - 1;
            end
            if (valid_input && !stop_input) begin
                write_index <= write_index + 1;
                data_array[write_index] <= data_input;
                data_size <= data_size + 1;
            end
        end
    end

endmodule

`endif  // ELASTIC BUFFER
