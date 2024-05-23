#include <entity/dfg.hpp>

entity::DFG::DFG(entity::DFGGraph dfg_graph)
    : entity::BaseGraphClass<entity::DFGNodeProperty, entity::DFGEdgeProperty,
                             entity::DFGGraphProperty>(dfg_graph){};
