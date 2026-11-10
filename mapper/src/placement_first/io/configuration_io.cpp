#include <boost/property_tree/ptree.hpp>
#include <mapper/placement_first/configuration.hpp>

namespace mapper {

void PlacementFirstConfig::ReadOptions(
    const boost::property_tree::ptree& tree) {
  CheckKnownOptions(tree, {});
}

void PlacementFirstConfig::WriteOptions(
    boost::property_tree::ptree& tree) const {}

}  // namespace mapper
