`ifndef PARAM
`define PARAM

// CGRA HW param
parameter CONTEXT_SIZE_BIT_LENGTH = 3;  // log CONTEXT_HW_SIZE
parameter CONTEXT_HW_SIZE = 8;
parameter ADDRESS_WIDTH = 8;  // log MEMORY_SIZE
parameter MEMORY_SIZE = 256;
parameter CONTEXT_SWITCH_CLK_SIZE = 3;

// CGRA data path param
parameter DATA_WIDTH = 32;
parameter OPERATION_BIT_LENGTH = 4;

// CGRA topology param
parameter NEIGHBOR_PE_NUM = 4;
parameter NEIGHBOR_PE_NUM_BIT_LENGTH = 2;

// CGRA execution param
parameter CONTEXT_SW_SIZE = 8;

`endif  // PARAM
