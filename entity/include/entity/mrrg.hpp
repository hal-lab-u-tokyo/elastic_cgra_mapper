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
  kwithController,
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
  bool is_raccoon;
  int loop_pe_pos;
  bool is_TM_raccoon;
  bool is_TM_raccoon_2;
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
  std::tuple<int, int, int> GetMRRGConfigId(int node_id);
    bool isSamePos(entity::MRRGConfig mrrg_config, const std::pair<int, int>& pos_normal, const std::pair<int, int>& pos_last_row) {
    if((pos_last_row.first == mrrg_config.row - 1 || pos_last_row.first == mrrg_config.row - 2 || pos_last_row.first == mrrg_config.row - 3) && TM_pe_positions[mrrg_config.row - 1 - pos_last_row.first][pos_last_row.second].first == pos_normal.first && TM_pe_positions[mrrg_config.row - 1 - pos_last_row.first][pos_last_row.second].second == pos_normal.second){
      return true;
    }
    return false;
  }
  std::vector<std::tuple<int, int, int>> GetTMSamePos(entity::MRRGConfig mrrg_config, const std::tuple<int, int, int>& normal_node_id){
    std::vector<std::tuple<int, int, int>> same_pos_vec;
    int normal_row = std::get<0>(normal_node_id);
    int normal_column = std::get<1>(normal_node_id);
    for(int j = 1; j <= 3; j++){
      for(int i = 0; i < mrrg_config.column; i++){
        std::pair<int, int> pos_last_row = std::make_pair(mrrg_config.row - j, i);
        std::pair<int, int> pos_normal = std::make_pair(normal_row, normal_column);
        if(isSamePos(mrrg_config, pos_normal, pos_last_row)){
          for(int k = 0; k < mrrg_config.context_size; k++){
            same_pos_vec.push_back(std::make_tuple(pos_last_row.first, pos_last_row.second, k));
          }
        }
      }
    }
    return same_pos_vec;
  }

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
  // Add a reverse map for config_id_to_node_id_map_
  std::unordered_map<int, std::tuple<int, int, int>> node_id_to_config_id_map_;

  // //TM5
  // std::vector<std::vector<std::pair<int, int>>> TM_pe_positions = {{std::make_pair(5, 3),
  //                                                     std::make_pair(5, 3),
  //                                                     std::make_pair(5, 3),
  //                                                     std::make_pair(5, 3),
  //                                                     std::make_pair(4, 4),
  //                                                     std::make_pair(4, 4),
  //                                                     std::make_pair(4, 4),
  //                                                     std::make_pair(4, 4)},
  //                                                   {std::make_pair(6, 5),
  //                                                     std::make_pair(6, 5),
  //                                                     std::make_pair(6, 5),
  //                                                     std::make_pair(6, 5),
  //                                                     std::make_pair(4, 6),
  //                                                     std::make_pair(4, 6),
  //                                                     std::make_pair(4, 6),
  //                                                     std::make_pair(4, 6)},
  //                                                   {std::make_pair(4, 7),
  //                                                     std::make_pair(4, 7),
  //                                                     std::make_pair(4, 7),
  //                                                     std::make_pair(4, 7),
  //                                                     std::make_pair(-1, -1),
  //                                                     std::make_pair(-1, -1),
  //                                                     std::make_pair(-1, -1),
  //                                                     std::make_pair(-1, -1)}};

  //TM4
  std::vector<std::vector<std::pair<int, int>>> TM_pe_positions = {{std::make_pair(5, 3),
                                                      std::make_pair(5, 3),
                                                      std::make_pair(5, 3),
                                                      std::make_pair(4, 4),
                                                      std::make_pair(4, 4),
                                                      std::make_pair(4, 4),
                                                      std::make_pair(6, 5),
                                                      std::make_pair(6, 5),
                                                    },
                                                    {std::make_pair(6, 5),
                                                      std::make_pair(4, 6),
                                                      std::make_pair(4, 6),
                                                      std::make_pair(4, 6),
                                                      std::make_pair(4, 7),
                                                      std::make_pair(4, 7),
                                                      std::make_pair(4, 7),
                                                      std::make_pair(-1, -1)},
                                                    {std::make_pair(-1, -1),
                                                      std::make_pair(-1, -1),
                                                      std::make_pair(-1, -1),
                                                      std::make_pair(-1, -1),
                                                      std::make_pair(-1, -1),
                                                      std::make_pair(-1, -1),
                                                      std::make_pair(-1, -1),
                                                      std::make_pair(-1, -1)}};
};
}  // namespace entity
