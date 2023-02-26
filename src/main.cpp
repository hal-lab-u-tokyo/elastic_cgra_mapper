#include <gurobi_c++.h>

#include <boost/array.hpp>
#include <boost/graph/adjacency_list.hpp>
#include <io/architecture_io.hpp>
#include <mapper/gurobi_mapper.hpp>

int main() {
  std::cout << "Hello world!" << std::endl;

  // test: boost adjacent vertices
  boost::adjacency_list<> g;
  boost::adjacency_list<>::vertex_descriptor v1 = boost::add_vertex(g);
  boost::adjacency_list<>::vertex_descriptor v2 = boost::add_vertex(g);
  boost::adjacency_list<>::vertex_descriptor v3 = boost::add_vertex(g);
  boost::adjacency_list<>::vertex_descriptor v4 = boost::add_vertex(g);

  boost::add_edge(v1, v2, g);
  boost::add_edge(v2, v3, g);
  boost::add_edge(v2, v4, g);

  boost::adjacency_list<>::adjacency_iterator vit, vend;
  std::tie(vit, vend) = boost::adjacent_vertices(v2, g);

  for (; vit != vend; vit++) {
    std::cout << *vit << std::endl;
  }
}