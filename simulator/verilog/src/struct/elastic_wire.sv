`ifndef ELASTIC_WIRE
`define ELASTIC_WIRE

typedef struct packed {
    logic valid;
    logic stop;
    logic [DATA_WIDTH - 1:0] data;
} ElasticWire;

`endif  // ELASTIC_WIRE
