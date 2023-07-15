`ifndef PARAM
`define PARAM

// CGRA HW param
parameter PE_COLUMN_SIZE = 4;
parameter PE_COLUMN_BIT_LENGTH = 2;  // log PE_WIDTH
parameter PE_ROW_SIZE = 4;
parameter PE_ROW_BIT_LENGTH = 2;  // log PE_HEIGHT
parameter CONTEXT_SIZE_BIT_LENGTH = 3;  // log CONTEXT_HW_SIZE
parameter CONTEXT_SIZE = 8;
parameter ADDRESS_WIDTH = 10;  // log MEMORY_SIZE
parameter MEMORY_SIZE = 1024;
parameter CONTEXT_SWITCH_CLK_SIZE = 3;

// CGRA data path param
parameter DATA_WIDTH = 32;
parameter OPERATION_BIT_LENGTH = 4;

// CGRA topology param
parameter NEIGHBOR_PE_NUM = 5;
parameter NEIGHBOR_PE_NUM_BIT_LENGTH = 3;

// param for Elastic CGRA
parameter ELASTIC_BUFFER_SIZE = 4;
parameter ELASTIC_BUFFER_SIZE_BIT_LENGTH = 2;

// param 
parameter ADD_CYCLE = 1;
parameter SUB_CYCLE = 1;
parameter MUL_CYCLE = 4;
parameter DIV_CYCLE = 4;
parameter CONST_CYCLE = 1;
parameter LOAD_CYCLE = 4;
parameter OUTPUT_CYCLE = 1;
parameter ROUTE_CYCLE = 1;

`endif  // PARAM
