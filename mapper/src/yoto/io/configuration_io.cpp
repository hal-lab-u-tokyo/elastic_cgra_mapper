#include <boost/property_tree/ptree.hpp>
#include <mapper/yoto/configuration.hpp>

namespace mapper {

void YOTOConfig::ReadOptions(const boost::property_tree::ptree& tree) {
  CheckKnownOptions(
      tree, {"max_trials", "seed_count", "routing_retry_count", "random_seed",
             "io_node_policy", "trial_seed_policy", "traversal_neighbor_policy",
             "candidate_scope_policy", "candidate_rank_policy",
             "use_yott_annotations", "trace_trials", "placement_only"});
  if (tree.get<std::string>("max_trials", "auto") == "auto")
    max_trials.reset();
  else
    max_trials = tree.get<int>("max_trials");
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
  if (tree.get<std::string>("io_node_policy", "auto") == "auto")
    io_node_policy.reset();
  else
    io_node_policy = tree.get<std::string>("io_node_policy");
  if (tree.get<std::string>("trial_seed_policy", "auto") == "auto")
    trial_seed_policy.reset();
  else
    trial_seed_policy = tree.get<std::string>("trial_seed_policy");
  if (tree.get<std::string>("traversal_neighbor_policy", "auto") == "auto")
    traversal_neighbor_policy.reset();
  else
    traversal_neighbor_policy =
        tree.get<std::string>("traversal_neighbor_policy");
  if (tree.get<std::string>("candidate_scope_policy", "auto") == "auto")
    candidate_scope_policy.reset();
  else
    candidate_scope_policy = tree.get<std::string>("candidate_scope_policy");
  if (tree.get<std::string>("candidate_rank_policy", "auto") == "auto")
    candidate_rank_policy.reset();
  else
    candidate_rank_policy = tree.get<std::string>("candidate_rank_policy");
  if (tree.get<std::string>("use_yott_annotations", "auto") == "auto")
    use_yott_annotations.reset();
  else
    use_yott_annotations = tree.get<bool>("use_yott_annotations");
  if (tree.get<std::string>("trace_trials", "auto") == "auto")
    trace_trials.reset();
  else
    trace_trials = tree.get<bool>("trace_trials");
  placement_only = tree.get<bool>("placement_only", false);
}

void YOTOConfig::WriteOptions(boost::property_tree::ptree& tree) const {
  if (max_trials)
    tree.put("max_trials", *max_trials);
  else
    tree.put("max_trials", "auto");
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
  if (io_node_policy)
    tree.put("io_node_policy", *io_node_policy);
  else
    tree.put("io_node_policy", "auto");
  if (trial_seed_policy)
    tree.put("trial_seed_policy", *trial_seed_policy);
  else
    tree.put("trial_seed_policy", "auto");
  if (traversal_neighbor_policy)
    tree.put("traversal_neighbor_policy", *traversal_neighbor_policy);
  else
    tree.put("traversal_neighbor_policy", "auto");
  if (candidate_scope_policy)
    tree.put("candidate_scope_policy", *candidate_scope_policy);
  else
    tree.put("candidate_scope_policy", "auto");
  if (candidate_rank_policy)
    tree.put("candidate_rank_policy", *candidate_rank_policy);
  else
    tree.put("candidate_rank_policy", "auto");
  if (use_yott_annotations)
    tree.put("use_yott_annotations", *use_yott_annotations);
  else
    tree.put("use_yott_annotations", "auto");
  if (trace_trials)
    tree.put("trace_trials", *trace_trials);
  else
    tree.put("trace_trials", "auto");
  tree.put("placement_only", placement_only);
}

}  // namespace mapper
