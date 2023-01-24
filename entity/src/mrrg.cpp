#include <entity/mrrg.hpp>

entity::MRRG::MRRG(entity::MRRGGraph mrrg_graph)
    : entity::BaseGraphClass<entity::MRRGNodeProperty, entity::MRRGEdgeProperty,
                             entity::MRRGGraphProperty>(mrrg_graph){};