`include "param.sv"
`include "data_memory.sv"
`include "elastic_PE.sv"

module ElasticCGRA (
    input wire clk,
    input wire reset_n,
    // config load if
    input [PE_ROW_BIT_LENGTH-1:0] config_PE_row_index,
    input [PE_COLUMN_BIT_LENGTH-1:0] config_PE_column_index,
    input ElasticConfigData config_data,
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
    output [DATA_WIDTH-1:0] pe_output[PE_ROW_SIZE][PE_COLUMN_SIZE][NEIGHBOR_PE_NUM],
    output [DATA_WIDTH-1:0] pe_input_array[PE_ROW_SIZE][PE_COLUMN_SIZE][NEIGHBOR_PE_NUM],
    // SELF protocol
    output pe_valid_output[PE_ROW_SIZE][PE_COLUMN_SIZE][NEIGHBOR_PE_NUM],
    output pe_stop_input[PE_ROW_SIZE][PE_COLUMN_SIZE][NEIGHBOR_PE_NUM],
    // DEBUG
    output [CONTEXT_SIZE_BIT_LENGTH-1:0] DEBUG_alu_context_id[PE_ROW_SIZE][PE_COLUMN_SIZE],
    output [OPERATION_BIT_LENGTH-1:0] DEBUG_alu_op[PE_ROW_SIZE][PE_COLUMN_SIZE],
    output [DATA_WIDTH-1:0] DEBUG_pe_output[PE_ROW_SIZE][PE_COLUMN_SIZE],
    output DEBUG_pe_mux_output_valid[PE_ROW_SIZE][PE_COLUMN_SIZE][2],
    output DEBUG_pe_mux_output_stop[PE_ROW_SIZE][PE_COLUMN_SIZE][2],
    output[INPUT_NUM_BIT_LENGTH-1:0] DEBUG_pe_mux_input_PE_index[PE_ROW_SIZE][PE_COLUMN_SIZE][2],
    output DEBUG_pe_buffer_input_stop[PE_ROW_SIZE][PE_COLUMN_SIZE],
    output DEBUG_pe_buffer_input_valid[PE_ROW_SIZE][PE_COLUMN_SIZE],
    output DEBUG_pe_fork_a_output_stop[PE_ROW_SIZE][PE_COLUMN_SIZE][INPUT_NUM],
    output DEBUG_pe_fork_a_output_valid[PE_ROW_SIZE][PE_COLUMN_SIZE][INPUT_NUM],
    output DEBUG_pe_join_output_stop[PE_ROW_SIZE][PE_COLUMN_SIZE],
    output DEBUG_pe_join_output_valid[PE_ROW_SIZE][PE_COLUMN_SIZE],
    output DEBUG_vailid_output[PE_ROW_SIZE][PE_COLUMN_SIZE],
    output [DATA_WIDTH-1:0] DEBUG_output_context_id[PE_ROW_SIZE][PE_COLUMN_SIZE],
);
    // data path
    wire [DATA_WIDTH-1:0] memory_read_data[PE_ROW_SIZE][PE_COLUMN_SIZE];
    wire [ADDRESS_WIDTH-1:0] memory_read_address[PE_ROW_SIZE][PE_COLUMN_SIZE];

    // Complete elastic links: producer drives data/valid, consumer drives stop.
    wire ElasticWire channels[PE_ROW_SIZE][PE_COLUMN_SIZE][NEIGHBOR_PE_NUM];

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
                assign DEBUG_pe_output[i][j] = channels[i][j][0].data;

                wire [DATA_WIDTH - 1:0] pe_input_data[NEIGHBOR_PE_NUM];
                wire tmp_pe_valid_input[NEIGHBOR_PE_NUM];
                wire tmp_pe_stop_input[NEIGHBOR_PE_NUM];
                wire tmp_pe_valid_output[NEIGHBOR_PE_NUM];
                wire tmp_pe_stop_output[NEIGHBOR_PE_NUM];

                for (genvar k = 0; k < NEIGHBOR_PE_NUM; k++) begin
                    assign channels[i][j][k].data = pe_output[i][j][k];
                    assign channels[i][j][k].valid = tmp_pe_valid_output[k];
                    assign tmp_pe_stop_output[k] = channels[i][j][k].stop;
                end
                assign pe_input_array[i][j]  = pe_input_data;
                assign pe_stop_input[i][j]   = tmp_pe_stop_input;
                assign pe_valid_output[i][j] = tmp_pe_valid_output;
                if (i > 0) begin
                    assign pe_input_data[0] = channels[i-1][j][1].data;
                    assign tmp_pe_valid_input[0] = channels[i-1][j][1].valid;
                    assign channels[i][j][0].stop = pe_stop_input[i-1][j][1];
                end
                if (i < PE_ROW_SIZE - 1) begin
                    assign pe_input_data[1] = channels[i+1][j][0].data;
                    assign tmp_pe_valid_input[1] = channels[i+1][j][0].valid;
                    assign channels[i][j][1].stop = pe_stop_input[i+1][j][0];
                end
                if (j > 0) begin
                    assign pe_input_data[2] = channels[i][j-1][3].data;
                    assign tmp_pe_valid_input[2] = channels[i][j-1][3].valid;
                    assign channels[i][j][2].stop = pe_stop_input[i][j-1][3];
                end
                if (j < PE_COLUMN_SIZE - 1) begin
                    assign pe_input_data[3] = channels[i][j+1][2].data;
                    assign tmp_pe_valid_input[3] = channels[i][j+1][2].valid;
                    assign channels[i][j][3].stop = pe_stop_input[i][j+1][2];
                end

                wire pe_write_config_data;
                assign pe_write_config_data = (config_PE_row_index === i) & (config_PE_column_index === j) & write_config_data;

                ElasticPE pe (
                    .clk(clk),
                    .reset_n(reset_n),
                    // config load if
                    .config_data(config_data),
                    .write_config_data(pe_write_config_data),
                    .config_index(config_index),
                    // execution param
                    .start_exec(start_exec),
                    .mapping_context_max_id(mapping_context_max_id),
                    // PE if
                    .pe_input_data(pe_input_data),
                    .pe_output_data(pe_output[i][j]),
                    // memory if
                    .memory_write_address(memory_write_address),
                    .memory_write(memory_write),
                    .memory_write_data(memory_write_data),
                    .memory_read_address(memory_read_address[i][j]),
                    .memory_read_data(memory_read_data[i][j]),
                    // SELF protocol
                    .valid_input(tmp_pe_valid_input),
                    .stop_input(tmp_pe_stop_input),
                    .valid_output(tmp_pe_valid_output),
                    .stop_output(tmp_pe_stop_output),
                    // DEBUG
                    .DEBUG_alu_context_id(DEBUG_alu_context_id[i][j]),
                    .DEBUG_alu_op(DEBUG_alu_op[i][j]),
                    .DEBUG_mux_output_valid(DEBUG_pe_mux_output_valid[i][j]),
                    .DEBUG_mux_output_stop(DEBUG_pe_mux_output_stop[i][j]),
                    .DEBUG_mux_input_PE_index(DEBUG_pe_mux_input_PE_index[i][j]),
                    .DEBUG_buffer_input_stop(DEBUG_pe_buffer_input_stop[i][j]),
                    .DEBUG_buffer_input_valid(DEBUG_pe_buffer_input_valid[i][j]),
                    .DEBUG_fork_a_output_stop(DEBUG_pe_fork_a_output_stop[i][j]),
                    .DEBUG_fork_a_output_valid(DEBUG_pe_fork_a_output_valid[i][j]),
                    .DEBUG_join_output_stop(DEBUG_pe_join_output_stop[i][j]),
                    .DEBUG_join_output_valid(DEBUG_pe_join_output_valid[i][j])
                );
            end
        end
    endgenerate
endmodule
