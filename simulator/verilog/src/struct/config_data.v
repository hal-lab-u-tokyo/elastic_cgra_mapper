`ifndef CONFIG_DATA
`define CONFIG_DATA

typedef struct packed {
    logic [NEIGHBOR_PE_NUM_BIT_LENGTH-1:0] input_PE_index_1;
    logic [NEIGHBOR_PE_NUM_BIT_LENGTH-1:0] input_PE_index_2;
    logic [OPERATION_BIT_LENGTH-1:0] op;
    logic [DATA_WIDTH - 1:0] const_data;
} ConfigData;

`endif  // CONFIG_DATA
