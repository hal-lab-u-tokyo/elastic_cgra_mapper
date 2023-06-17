`include "param.v"

module DataMemory (
    input clk,
    input reset_n,
    input [ADDRESS_WIDTH-1:0] address,
    input write,
    input [DATA_WIDTH-1:0] input_data,
    output reg [DATA_WIDTH-1:0] output_data
);

    reg [DATA_WIDTH-1:0] r_memory[MEMORY_SIZE - 1:0];

    always_ff @(posedge clk, negedge reset_n) begin
        if (!reset_n) begin
            integer i;
            for (i = 0; i < MEMORY_SIZE; i++) begin
                r_memory[i] = 0;
            end
        end else begin
            if (write) begin
                r_memory[address] <= input_data;
            end else begin
                output_data <= r_memory[address];
            end
        end
    end

endmodule