#include <boost/property_tree/ptree.hpp>
#include <mapper/sa/configuration.hpp>

namespace mapper {

void SAConfig::ReadOptions(const boost::property_tree::ptree& tree) {
  CheckKnownOptions(tree,
                    {"seed_count", "routing_retry_count", "random_seed",
                     "max_iterations", "io_node_policy", "placement_only"});
  if (tree.get<std::string>("seed_count", "auto") == "auto")
    seed_count.reset();
  else
    seed_count = tree.get<int>("seed_count");
  if (tree.get<std::string>("routing_retry_count", "auto") == "auto")
    routing_retry_count.reset();
  else
    routing_retry_count = tree.get<int>("routing_retry_count");
  if (tree.get<std::string>("random_seed", "auto") == "auto")
    random_seed.reset();
  else
    random_seed = tree.get<int>("random_seed");
  if (tree.get<std::string>("max_iterations", "auto") == "auto")
    max_iterations.reset();
  else
    max_iterations = tree.get<int>("max_iterations");
  if (tree.get<std::string>("io_node_policy", "auto") == "auto")
    io_node_policy.reset();
  else
    io_node_policy = tree.get<std::string>("io_node_policy");
  placement_only = tree.get<bool>("placement_only", false);
}

void SAConfig::WriteOptions(boost::property_tree::ptree& tree) const {
  if (seed_count)
    tree.put("seed_count", *seed_count);
  else
    tree.put("seed_count", "auto");
  if (routing_retry_count)
    tree.put("routing_retry_count", *routing_retry_count);
  else
    tree.put("routing_retry_count", "auto");
  if (random_seed)
    tree.put("random_seed", *random_seed);
  else
    tree.put("random_seed", "auto");
  if (max_iterations)
    tree.put("max_iterations", *max_iterations);
  else
    tree.put("max_iterations", "auto");
  if (io_node_policy)
    tree.put("io_node_policy", *io_node_policy);
  else
    tree.put("io_node_policy", "auto");
  tree.put("placement_only", placement_only);
}

}  // namespace mapper
