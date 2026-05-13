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

void entity::Database::LimitMappingNum(int mapping_num_limit) {
  if (mapping_num_limit == -1 || mapping_vec_.size() <= mapping_num_limit) {
    return;
  }

  std::sort(mapping_vec_.begin(), mapping_vec_.end(),
            [](const Mapping& a, const Mapping& b) {
              return a.GetOpRate() > b.GetOpRate();
            });
  mapping_vec_.resize(mapping_num_limit);
  return;
}
