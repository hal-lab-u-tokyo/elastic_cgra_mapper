#include "remapper/remapper.hpp"

#include <Eigen/Eigen>

#include "remapper/combination_counter.hpp"
#include "remapper/algorithm/dp_elastic_remapper.hpp"
#include "remapper/algorithm/full_search_elastic_remapper.hpp"
#include "remapper/algorithm/greedy_elastic_remapper.hpp"
#include "remapper/mapping_concater.hpp"
#include "remapper/mapping_transform_op.hpp"
#include "remapper/rotater.hpp"
#include "time.h"


std::pair<bool, entity::Mapping> remapper::Remapper::ElasticRemapping(
    const std::vector<entity::Mapping>& mapping_vec,
    const entity::MRRGConfig& target_mrrg_config, const int target_parallel_num,
    std::ofstream& log_file, remapper::RemappingMode mode) {
  switch (mode) {
    case RemappingMode::FullSearch:
      return remapper::FullSearchElasticRemapping(mapping_vec, target_mrrg_config,
                                        target_parallel_num, log_file);
    case RemappingMode::Greedy:
      return remapper::GreedyElasticRemapping(mapping_vec, target_mrrg_config,
                                    target_parallel_num, log_file);
    case RemappingMode::DP:
      return remapper::DPElasticRemapping(mapping_vec, target_mrrg_config,
                                target_parallel_num, log_file);
  }
}
