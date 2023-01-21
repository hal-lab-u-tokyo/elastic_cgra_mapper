#include <entity/graph.hpp>

entity::BaseGraphClass::BaseGraphClass(Graph graph) { graph_ = graph; }

entity::Graph entity::BaseGraphClass::GetGraph() const { return graph_; }
