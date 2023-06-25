`ifndef DATA_MEMORY
`define DATA_MEMORY
`include "param.v"

module DataMemory (
    input clk,
    input reset_n,
    input [ADDRESS_WIDTH-1:0] write_address,
    input [ADDRESS_WIDTH-1:0] read_address[PE_ROW_SIZE][PE_COLUMN_SIZE],
    input write,
    input [DATA_WIDTH-1:0] write_data,
    output reg [DATA_WIDTH-1:0] read_data[PE_ROW_SIZE][PE_COLUMN_SIZE]
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
                r_memory[write_address] <= write_data;
            end else begin
                integer i, j;
                for (i = 0; i < PE_ROW_SIZE; i++) begin
                    for (j = 0; j < PE_COLUMN_SIZE; j++) begin
                        read_data[i][j] <= r_memory[read_address[i][j]];
                    end
                end
            end
        end
    end

endmodule

`endif  // DATA_MEMORY
