#pragma once

#include <gurobi_c++.h>

#include <mapper/mapping_status.hpp>

namespace mapper {

// Preserve the existing acceptance policy: OPTIMAL and SUBOPTIMAL are accepted;
// other termination states need an incumbent and accept_feasible_solution.
inline MappingStatus ClassifyILPResult(int solver_status, int solution_count,
                                       bool accept_feasible_solution) {
  if (solution_count <= 0 || solver_status == GRB_INFEASIBLE)
    return MappingStatus::kFailure;
  if (solver_status == GRB_OPTIMAL) return MappingStatus::kOptimal;
  if (solver_status == GRB_SUBOPTIMAL) return MappingStatus::kSuboptimal;
  return accept_feasible_solution ? MappingStatus::kOptimalityUnknown
                                  : MappingStatus::kFailure;
}

}  // namespace mapper
