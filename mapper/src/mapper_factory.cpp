#include <mapper/mapper_factory.hpp>

#include <cstdlib>
#include <iostream>
#include <mapper/gurobi_mapper.hpp>
#include <mapper/gurobi_placement_mapper.hpp>
#include <mapper/placement_first_heuristic_mapper.hpp>

std::unique_ptr<mapper::IILPMapper> mapper::CreateMapper(
    entity::AlgorithmType algorithm,
    const std::shared_ptr<entity::DFG> dfg_ptr,
    const std::shared_ptr<entity::MRRG> mrrg_ptr) {
  switch (algorithm) {
    case entity::AlgorithmType::kILPMapper:
      return std::unique_ptr<mapper::IILPMapper>(
          mapper::GurobiILPMapper().CreateMapper(dfg_ptr, mrrg_ptr));
    case entity::AlgorithmType::kPlacementILPMapper:
      return std::unique_ptr<mapper::IILPMapper>(
          mapper::GurobiPlacementILPMapper().CreateMapper(dfg_ptr, mrrg_ptr));
    case entity::AlgorithmType::kPlacementFirstHeuristicMapper:
      return std::unique_ptr<mapper::IILPMapper>(
          mapper::PlacementFirstHeuristicMapper().CreateMapper(dfg_ptr,
                                                               mrrg_ptr));
  }

  std::cerr << "Invalid algorithm type in mapper config: "
            << static_cast<int>(algorithm) << std::endl;
  std::abort();
}
