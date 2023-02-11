#include <entity/mapping.hpp>

entity::Mapping::Mapping(entity::MRRG mrrg, entity::DFG dfg)
    : config_id_to_dfg_node_id_({}), config_map_({}) {
  mrrg_ptr_ = std::make_shared<entity::MRRG>();
  dfg_ptr_ = std::make_shared<entity::DFG>();

  *mrrg_ptr_ = mrrg;
  *dfg_ptr_ = dfg;
};