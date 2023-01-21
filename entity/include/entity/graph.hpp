#include <boost/graph/adjacency_list.hpp>

namespace entity {
typedef boost::adjacency_list<boost::vecS, boost::vecS, boost::bidirectionalS>
    Graph;
typedef std::pair<int, int> Edge;

class BaseGraphClass {
 public:
  BaseGraphClass(Graph graph);
  Graph GetGraph() const;

 private:
  Graph graph_;
};
}  // namespace entity