#pragma once

#include <entity/dfg.hpp>
#include <entity/graph.hpp>
#include <map>
#include <tuple>

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
  kOneEnd,
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

  MRRGConfig GetMRRGConfig() const;
  int GetMRRGNodeId(int row_id, int column_id, int context_id);

 private:
  struct HashTuple_ {
   public:
    size_t operator()(const std::tuple<int, int, int>& tuple) const {
      int a, b, c;
      std::tie(a, b, c) = tuple;
      return a * 1000 + b * 1000 + c * 1000;
    }
  };
  std::unordered_map<std::tuple<int, int, int>, int, HashTuple_>
      config_id_to_node_id_map_;
};
}  // namespace entity