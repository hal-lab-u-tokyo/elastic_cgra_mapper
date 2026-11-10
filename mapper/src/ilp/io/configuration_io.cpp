#include <boost/property_tree/ptree.hpp>
#include <mapper/ilp/configuration.hpp>

namespace mapper {

void ILPConfig::ReadOptions(const boost::property_tree::ptree& tree) {
  CheckKnownOptions(tree, {"accept_feasible_solution"});
  accept_feasible_solution = tree.get<bool>("accept_feasible_solution", true);
}

void ILPConfig::WriteOptions(boost::property_tree::ptree& tree) const {
  tree.put("accept_feasible_solution", accept_feasible_solution);
}

}  // namespace mapper
