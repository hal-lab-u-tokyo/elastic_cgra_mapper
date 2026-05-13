#include <entity/database.hpp>

int entity::Database::GetMinMappingOpNum() const {
  int min_mapping_op_num = INT_MAX;
  for (const auto& mapping : mapping_vec_) {
    int mapping_op_num = mapping.GetOpNum();

    if (mapping_op_num < min_mapping_op_num) {
      min_mapping_op_num = mapping_op_num;
    }
  }
  return min_mapping_op_num;
}
