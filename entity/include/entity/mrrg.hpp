#pragma once

#include <entity/dfg.hpp>
#include <entity/graph.hpp>

namespace entity {
struct MRRGNodeProperty {
  bool is_memory_accessible;

  int local_reg_size;
  int context_size;

  std::pair<int, int> position_id;
  int context_id;

  std::vector<OpType> supported_operations;
};

struct MRRGEdgeProperty {};

enum MRRGCGRAType {
  kDefault,
  kElastic,
};

enum MRRGMemoryIOType {
  kAll,
  kBothEnds,
};

enum MRRGNetworkType {
  kOrthogonal,
  kDiagonal,
};

struct MRRGGraphProperty {
  int row_num;
  int column_num;
  MRRGMemoryIOType memory_io;
  MRRGCGRAType cgra_type;
  MRRGNetworkType network_type;
};

struct MRRGConfig {
  int column;
  int row;
  MRRGMemoryIOType memory_io;
  MRRGCGRAType cgra_type;
  MRRGNetworkType network_type;
  int local_reg_size;
  int context_size;
};

MRRGCGRAType MRRGCGRATypeFromString(std::string cgra_type_string);
std::string MRRGCGRATypeToString(MRRGCGRAType cgra_type);
MRRGMemoryIOType MRRGMemoryIOTypeFromString(std::string memory_io_type_string);
std::string MRRGMemoryIoTypeToString(MRRGMemoryIOType memory_io_type);
MRRGNetworkType MRRGNetworkTypeFromString(std::string network_type_string);
std::string MRRGNetworkTypeToString(MRRGNetworkType network_type);

typedef BaseGraph<MRRGNodeProperty, MRRGEdgeProperty, MRRGGraphProperty>
    MRRGGraph;

class MRRG : public BaseGraphClass<MRRGNodeProperty, MRRGEdgeProperty,
                                   MRRGGraphProperty> {
 public:
  using BaseGraphClass<MRRGNodeProperty, MRRGEdgeProperty,
                       MRRGGraphProperty>::BaseGraphClass;
  MRRG(MRRGGraph mrrg_graph);
  MRRG(MRRGConfig mrrg_config);

  MRRGConfig GetMRRGConfig();
};
}  // namespace entity