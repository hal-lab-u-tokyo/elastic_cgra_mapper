`include "param.v"
`include "data_memory.v"
`include "synchronous_PE.v"

module CGRA (
    input wire clk,
    input wire reset_n,
    // config load if
    input [PE_ROW_BIT_LENGTH-1:0] config_PE_row_index,
    input [PE_COLUMN_BIT_LENGTH-1:0] config_PE_column_index,
    input [INPUT_NUM_BIT_LENGTH-1:0] config_input_PE_index_1,
    input [INPUT_NUM_BIT_LENGTH-1:0] config_input_PE_index_2,
    input [OPERATION_BIT_LENGTH-1:0] config_op,
    input [DATA_WIDTH-1:0] config_const_data,
    input write_config_data,
    input [CONTEXT_SIZE_BIT_LENGTH-1:0] config_index,
    // execution param
    input start_exec,
    input [CONTEXT_SIZE_BIT_LENGTH-1:0] mapping_context_max_id,
    // memory if
    inout [ADDRESS_WIDTH-1:0] memory_write_address,
    inout memory_write,
    inout [DATA_WIDTH-1:0] memory_write_data,
    // output
    output [DATA_WIDTH-1:0] pe_output[PE_ROW_SIZE][PE_COLUMN_SIZE],
    output [DATA_WIDTH-1:0] pe_input_array[PE_ROW_SIZE][PE_COLUMN_SIZE][INPUT_NUM],
    // DEBUG
    output [DATA_WIDTH-1:0] DEBUG_input_PE_data_1[PE_ROW_SIZE][PE_COLUMN_SIZE],
    output [DATA_WIDTH-1:0] DEBUG_input_PE_data_2[PE_ROW_SIZE][PE_COLUMN_SIZE],
    output [INPUT_NUM_BIT_LENGTH-1:0] DEBUG_input_PE_index_1[PE_ROW_SIZE][PE_COLUMN_SIZE],
    output [INPUT_NUM_BIT_LENGTH-1:0] DEBUG_input_PE_index_2[PE_ROW_SIZE][PE_COLUMN_SIZE],
    output DEBUG_write_config_data[PE_ROW_SIZE][PE_COLUMN_SIZE],
    output [ADDRESS_WIDTH-1:0] DEBUG_memory_read_address[PE_ROW_SIZE][PE_COLUMN_SIZE]
);
    // data path
    wire [DATA_WIDTH-1:0] memory_read_data[PE_ROW_SIZE][PE_COLUMN_SIZE];
    wire [ADDRESS_WIDTH-1:0] memory_read_address[PE_ROW_SIZE][PE_COLUMN_SIZE];
    assign DEBUG_memory_read_address = memory_read_address;

    // module
    DataMemory data_memory (
        .clk(clk),
        .reset_n(reset_n),
        .write_address(memory_write_address),
        .read_address(memory_read_address),
        .write(memory_write),
        .write_data(memory_write_data),
        .read_data(memory_read_data)
    );

    genvar i, j;
    generate
        for (i = 0; i < PE_ROW_SIZE; i++) begin : GenerateProcessingElementI
            for (
                j = 0; j < PE_COLUMN_SIZE; j++
            ) begin : GenerateProcessingElementJ
                wire [DATA_WIDTH - 1:0] pe_input_data[INPUT_NUM];
                assign pe_input_array[i][j] = pe_input_data;
                if (i > 0) begin
                    assign pe_input_data[0] = pe_output[i-1][j];
                end
                if (i < PE_ROW_SIZE - 1) begin
                    assign pe_input_data[1] = pe_output[i+1][j];
                end
                if (j > 0) begin
                    assign pe_input_data[2] = pe_output[i][j-1];
                end
                if (j < PE_COLUMN_SIZE - 1) begin
                    assign pe_input_data[3] = pe_output[i][j+1];
                end
                assign pe_input_data[4] = pe_output[i][j];

                wire pe_write_config_data;
                assign pe_write_config_data = (config_PE_row_index === i) & (config_PE_column_index === j) & write_config_data;

                assign DEBUG_write_config_data[i][j] = pe_write_config_data;

                PE pe (
                    .clk(clk),
                    .reset_n(reset_n),
                    .config_input_PE_index_1(config_input_PE_index_1),
                    .config_input_PE_index_2(config_input_PE_index_2),
                    .config_op(config_op),
                    .config_const_data(config_const_data),
                    .write_config_data(pe_write_config_data),
                    .config_index(config_index),
                    .pe_input_data(pe_input_data),
                    .pe_output_data(pe_output[i][j]),
                    .memory_write_address(memory_write_address),
                    .memory_write(memory_write),
                    .memory_write_data(memory_write_data),
                    .memory_read_address(memory_read_address[i][j]),
                    .memory_read_data(memory_read_data[i][j]),
                    .start_exec(start_exec),
                    .mapping_context_max_id(mapping_context_max_id),
                    .DEBUG_input_PE_data_1(DEBUG_input_PE_data_1[i][j]),
                    .DEBUG_input_PE_data_2(DEBUG_input_PE_data_2[i][j]),
                    .DEBUG_input_PE_index_1(DEBUG_input_PE_index_1[i][j]),
                    .DEBUG_input_PE_index_2(DEBUG_input_PE_index_2[i][j])
                );
            end
        end
    endgenerate
endmodule
