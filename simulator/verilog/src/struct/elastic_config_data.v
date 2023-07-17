`ifndef ELASTIC_CONFIG_DATA
`define ELASTIC_CONFIG_DATA

typedef struct packed {
    logic [INPUT_NUM_BIT_LENGTH-1:0] input_PE_index_1;
    logic [INPUT_NUM_BIT_LENGTH-1:0] input_PE_index_2;
    logic [NEIGHBOR_PE_NUM-1:0] output_PE_index;
    logic [OPERATION_BIT_LENGTH-1:0] op;
    logic [DATA_WIDTH-1:0] const_data;
} ElasticConfigData;

`endif  // ELASTIC_CONFIG_DATA
