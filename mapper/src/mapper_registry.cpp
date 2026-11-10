#include <initializer_list>
#include <mapper/ilp/configuration.hpp>
#include <mapper/ilp/connectivity_path_ilp_mapper.hpp>
#include <mapper/ilp/ilp_mapper.hpp>
#include <mapper/ilp/ilp_placement_mapper.hpp>
#include <mapper/mapper_factory.hpp>
#include <mapper/placement_first/configuration.hpp>
#include <mapper/placement_first/routed_placement_first_mapper.hpp>
#include <mapper/prisa/configuration.hpp>
#include <mapper/prisa/grid_prisa_mapper.hpp>
#include <mapper/prisa/prisa_mapper.hpp>
#include <mapper/prisa/routed_prisa_mapper.hpp>
#include <mapper/sa/configuration.hpp>
#include <mapper/sa/grid_sa_mapper.hpp>
#include <mapper/sa/routed_sa_mapper.hpp>
#include <mapper/sa/sa_mapper.hpp>
#include <mapper/yoto/configuration.hpp>
#include <mapper/yoto/grid_yoto_mapper.hpp>
#include <mapper/yoto/routed_yoto_mapper.hpp>
#include <mapper/yoto/routed_yoto_with_fallback_mapper.hpp>
#include <mapper/yoto/yoto_mapper.hpp>
#include <mapper/yott/configuration.hpp>
#include <mapper/yott/grid_yott_mapper.hpp>
#include <mapper/yott/routed_yott_mapper.hpp>
#include <mapper/yott/routed_yott_with_fallback_mapper.hpp>
#include <mapper/yott/yott_mapper.hpp>
#include <mapper/yott_core/configuration.hpp>
#include <mapper/yott_core/grid_yott_core_mapper.hpp>
#include <mapper/yott_core_repair/configuration.hpp>
#include <mapper/yott_core_repair/grid_yott_core_repair_mapper.hpp>

namespace {

template <typename MapperT, typename ConfigT>
void RegisterNames(std::initializer_list<const char*> names,
                   std::initializer_list<const char*> consumed_options) {
  const std::set<std::string> supported(consumed_options.begin(),
                                        consumed_options.end());
  for (const char* name : names) {
    mapper::RegisterMapper(
        name,
        [](const std::shared_ptr<entity::DFG>& dfg,
           const std::shared_ptr<entity::MRRG>& mrrg) {
          return std::make_unique<MapperT>(dfg, mrrg);
        },
        [supported] {
          auto config = std::make_unique<ConfigT>();
          config->supported_options = supported;
          return config;
        });
  }
}

bool RegisterBuiltInMappers() {
  RegisterNames<mapper::ILPMapper, mapper::ILPConfig>(
      {"ILP", "ILPMapper", "GurobiILPMapper", "FullRoutingILPMapper"},
      {"accept_feasible_solution"});
  // Canonical algorithm names select the implementation from the input MRRG.
  RegisterNames<mapper::SAMapper, mapper::SAConfig>(
      {"SA", "SAMapper"}, {"seed_count", "random_seed", "routing_retry_count",
                           "max_iterations", "placement_only"});
  RegisterNames<mapper::PRISAMapper, mapper::PRISAConfig>(
      {"PRISA", "PRISAMapper"},
      {"seed_count", "random_seed", "routing_retry_count", "max_iterations",
       "placement_only"});
  RegisterNames<mapper::YOTOMapper, mapper::YOTOConfig>(
      {"YOTO", "YOTOMapper"},
      {"seed_count", "random_seed", "routing_retry_count", "max_trials",
       "placement_only"});
  RegisterNames<mapper::YOTTMapper, mapper::YOTTConfig>(
      {"YOTT", "YOTTMapper"},
      {"seed_count", "random_seed", "routing_retry_count", "max_trials",
       "placement_only"});

  // Legacy explicit-variant aliases for reproducible experiment configs.
  RegisterNames<mapper::PlacementILPMapper, mapper::ILPConfig>(
      {"PlacementILPMapper", "Placement2DILPMapper"},
      {"accept_feasible_solution"});
  RegisterNames<mapper::Placement2DYOTOMapper, mapper::YOTOConfig>(
      {"Placement2DYOTOMapper", "Placement2DYOTO"},
      {"seed_count", "random_seed", "routing_retry_count", "max_trials",
       "placement_only"});
  RegisterNames<mapper::Placement2DYOTTMapper, mapper::YOTTConfig>(
      {"Placement2DYOTTMapper", "Placement2DYOTT"},
      {"seed_count", "random_seed", "routing_retry_count", "max_trials",
       "placement_only"});
  RegisterNames<mapper::Placement2DPaperGuidedArrayYOTOMapper,
                mapper::YOTOConfig>(
      {"Placement2DPaperGuidedArrayYOTOMapper",
       "Placement2DPaperGuidedArrayYOTO", "Placement2DFaithfulArrayYOTOMapper",
       "Placement2DFaithfulArrayYOTO"},
      {"seed_count", "random_seed", "io_node_policy", "max_trials",
       "traversal_neighbor_policy", "candidate_scope_policy",
       "candidate_rank_policy", "use_yott_annotations"});
  RegisterNames<mapper::Placement2DPaperGuidedArrayYOTTMapper,
                mapper::YOTTConfig>(
      {"Placement2DPaperGuidedArrayYOTTMapper",
       "Placement2DPaperGuidedArrayYOTT", "Placement2DFaithfulArrayYOTTMapper",
       "Placement2DFaithfulArrayYOTT"},
      {"seed_count", "random_seed", "io_node_policy", "max_trials",
       "traversal_neighbor_policy", "candidate_scope_policy",
       "candidate_rank_policy", "use_yott_annotations"});
  RegisterNames<mapper::Placement2DCPUMappingYOTOMapper, mapper::YOTOConfig>(
      {"Placement2DCPUMappingYOTOMapper", "Placement2DCPUMappingYOTO"},
      {"seed_count", "random_seed", "io_node_policy", "max_trials",
       "traversal_neighbor_policy", "candidate_scope_policy",
       "candidate_rank_policy", "use_yott_annotations", "trial_seed_policy",
       "trace_trials"});
  RegisterNames<mapper::Placement2DCPUMappingYOTTMapper, mapper::YOTTConfig>(
      {"Placement2DCPUMappingYOTTMapper", "Placement2DCPUMappingYOTT"},
      {"seed_count", "random_seed", "io_node_policy", "max_trials",
       "traversal_neighbor_policy", "candidate_scope_policy",
       "candidate_rank_policy", "use_yott_annotations", "trial_seed_policy",
       "trace_trials"});
  RegisterNames<mapper::Placement2DCPUMappingYOTTCoreMapper,
                mapper::YOTTCoreConfig>(
      {"Placement2DCPUMappingYOTTCoreMapper", "Placement2DCPUMappingYOTTCore"},
      {"seed_count", "random_seed", "io_node_policy", "max_trials",
       "trial_seed_policy", "traversal_order_policy",
       "traversal_neighbor_policy", "use_yott_annotations", "trace_trials"});
  RegisterNames<mapper::Placement2DCPUMappingYOTTCoreRepairMapper,
                mapper::YOTTCoreRepairConfig>(
      {"Placement2DCPUMappingYOTTCoreRepairMapper",
       "Placement2DCPUMappingYOTTCoreRepair",
       "Placement2DCPUMappingYOTTCoreBottleneckRepairMapper",
       "Placement2DCPUMappingYOTTCoreBottleneckRepair"},
      {"seed_count", "random_seed", "io_node_policy", "max_trials",
       "trial_seed_policy", "traversal_order_policy",
       "traversal_neighbor_policy", "use_yott_annotations", "trace_trials",
       "max_iterations", "elite_placement_count", "candidate_rank_policy"});
  RegisterNames<mapper::Placement2DSAMapper, mapper::SAConfig>(
      {"Placement2DSAMapper", "Placement2DSA"},
      {"seed_count", "random_seed", "routing_retry_count", "max_iterations",
       "placement_only"});
  RegisterNames<mapper::Placement2DArraySAMapper, mapper::SAConfig>(
      {"Placement2DArraySAMapper", "Placement2DArraySA"},
      {"seed_count", "random_seed", "io_node_policy", "max_iterations"});
  RegisterNames<mapper::Placement2DPRISAMapper, mapper::PRISAConfig>(
      {"Placement2DPRISAMapper", "Placement2DPRISA"},
      {"seed_count", "random_seed", "routing_retry_count", "max_iterations",
       "placement_only"});
  RegisterNames<mapper::Placement2DPRISANoSISMapper, mapper::PRISAConfig>(
      {"Placement2DPRISANoSISMapper", "Placement2DPRISANoSIS"},
      {"seed_count", "random_seed", "routing_retry_count", "max_iterations",
       "placement_only"});
  RegisterNames<mapper::Placement2DCostAwarePRISAMapper, mapper::PRISAConfig>(
      {"Placement2DCostAwarePRISAMapper", "Placement2DCostAwarePRISA"},
      {"seed_count", "random_seed", "routing_retry_count", "max_iterations",
       "max_trials", "placement_only"});
  RegisterNames<mapper::Placement2DArrayPRISAMapper, mapper::PRISAConfig>(
      {"Placement2DArrayPRISAMapper", "Placement2DArrayPRISA"},
      {"seed_count", "random_seed", "io_node_policy", "max_iterations"});
  RegisterNames<mapper::Placement2DArrayPRISANoSISMapper, mapper::PRISAConfig>(
      {"Placement2DArrayPRISANoSISMapper", "Placement2DArrayPRISANoSIS"},
      {"seed_count", "random_seed", "io_node_policy", "max_iterations"});
  RegisterNames<mapper::Placement2DArrayCostAwarePRISAMapper,
                mapper::PRISAConfig>(
      {"Placement2DArrayCostAwarePRISAMapper",
       "Placement2DArrayCostAwarePRISA"},
      {"seed_count", "random_seed", "io_node_policy", "max_iterations",
       "max_trials"});

  // Modulo placement and routing.
  RegisterNames<mapper::ConnectivityPathILPMapper, mapper::ILPConfig>(
      {"ConnectivityPathILPMapper", "ConnectivityBasedILPMapper"},
      {"accept_feasible_solution"});
  RegisterNames<mapper::ModuloPlacementFirstMapper,
                mapper::PlacementFirstConfig>(
      {"ModuloPlacementFirstMapper", "ModuloPlacementFirst",
       "PlacementFirstHeuristicMapper"},
      {});
  RegisterNames<mapper::ModuloSAMapper, mapper::SAConfig>(
      {"ModuloSAMapper", "ModuloSA"},
      {"seed_count", "random_seed", "routing_retry_count", "max_iterations"});
  RegisterNames<mapper::ModuloYOTOMapper, mapper::YOTOConfig>(
      {"ModuloYOTOMapper", "ModuloYOTO"},
      {"seed_count", "random_seed", "routing_retry_count", "max_trials"});
  RegisterNames<mapper::ModuloPhysicalYOTOMapper, mapper::YOTOConfig>(
      {"ModuloPhysicalYOTOMapper", "ModuloPhysicalYOTO"},
      {"seed_count", "random_seed", "routing_retry_count", "max_trials"});
  RegisterNames<mapper::ModuloYOTOWithFallbackMapper, mapper::YOTOConfig>(
      {"ModuloYOTOWithFallbackMapper", "ModuloYOTOWithFallback"},
      {"seed_count", "random_seed", "routing_retry_count", "max_trials"});
  RegisterNames<mapper::ModuloYOTTMapper, mapper::YOTTConfig>(
      {"ModuloYOTTMapper", "ModuloYOTT"},
      {"seed_count", "random_seed", "routing_retry_count", "max_trials"});
  RegisterNames<mapper::ModuloPhysicalYOTTMapper, mapper::YOTTConfig>(
      {"ModuloPhysicalYOTTMapper", "ModuloPhysicalYOTT"},
      {"seed_count", "random_seed", "routing_retry_count", "max_trials"});
  RegisterNames<mapper::ModuloYOTTWithFallbackMapper, mapper::YOTTConfig>(
      {"ModuloYOTTWithFallbackMapper", "ModuloYOTTWithFallback"},
      {"seed_count", "random_seed", "routing_retry_count", "max_trials"});
  RegisterNames<mapper::ModuloPhysicalPRISAMapper, mapper::PRISAConfig>(
      {"ModuloPhysicalPRISAMapper", "ModuloPhysicalPRISA"},
      {"seed_count", "random_seed", "routing_retry_count", "max_iterations"});
  RegisterNames<mapper::ModuloPhysicalPRISAManhattanMapper,
                mapper::PRISAConfig>(
      {"ModuloPhysicalPRISAManhattanMapper", "ModuloPhysicalPRISAManhattan"},
      {"seed_count", "random_seed", "routing_retry_count", "max_iterations"});
  return true;
}

const bool kBuiltInMappersRegistered = RegisterBuiltInMappers();

}  // namespace
