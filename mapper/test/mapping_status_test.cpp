#include <gtest/gtest.h>

#include <mapper/ilp/result_status.hpp>
#include <mapper/mapper.hpp>
#include <type_traits>

TEST(MappingStatusTest, enum_and_string_contract) {
  using mapper::MappingStatus;
  static_assert(!std::is_constructible_v<mapper::MappingResult, bool,
                                         const entity::Mapping&, double>);
  for (auto status :
       {MappingStatus::kOptimal, MappingStatus::kSuboptimal,
        MappingStatus::kOptimalityUnknown, MappingStatus::kFailure}) {
    EXPECT_EQ(
        mapper::MappingStatusFromString(mapper::MappingStatusToString(status)),
        status);
    EXPECT_EQ(mapper::HasMapping(status), status != MappingStatus::kFailure);
  }
  EXPECT_THROW(mapper::MappingStatusFromString("success"),
               std::invalid_argument);
  EXPECT_THROW(mapper::MappingStatusToString(static_cast<MappingStatus>(99)),
               std::invalid_argument);
}

TEST(MappingStatusTest, solver_outcome_is_separate_from_acceptance_policy) {
  using mapper::ClassifyILPResult;
  using mapper::MappingStatus;
  for (bool accept : {false, true}) {
    EXPECT_EQ(ClassifyILPResult(GRB_OPTIMAL, 1, accept),
              MappingStatus::kOptimal);
    // Preserve existing policy accepting solver-reported SUBOPTIMAL even in
    // strict mode; this label is not proof of a strictly worse objective.
    EXPECT_EQ(ClassifyILPResult(GRB_SUBOPTIMAL, 1, accept),
              MappingStatus::kSuboptimal);
    EXPECT_EQ(ClassifyILPResult(GRB_INFEASIBLE, 0, accept),
              MappingStatus::kFailure);
    EXPECT_EQ(ClassifyILPResult(GRB_OPTIMAL, 0, accept),
              MappingStatus::kFailure);
    EXPECT_EQ(ClassifyILPResult(GRB_SUBOPTIMAL, 0, accept),
              MappingStatus::kFailure);
    EXPECT_EQ(ClassifyILPResult(GRB_TIME_LIMIT, 0, accept),
              MappingStatus::kFailure);
  }
  EXPECT_EQ(ClassifyILPResult(GRB_TIME_LIMIT, 1, true),
            MappingStatus::kOptimalityUnknown);
  EXPECT_EQ(ClassifyILPResult(GRB_NODE_LIMIT, 1, true),
            MappingStatus::kOptimalityUnknown);
  EXPECT_EQ(ClassifyILPResult(GRB_TIME_LIMIT, 1, false),
            MappingStatus::kFailure);
}

TEST(MappingStatusTest, actual_solver_incumbent_limit_remains_uncertified) {
  GRBEnv environment(true);
  environment.set(GRB_IntParam_OutputFlag, 0);
  environment.start();
  GRBModel model(environment);
  model.set(GRB_IntParam_Threads, 1);
  model.set(GRB_IntParam_Presolve, 0);
  model.set(GRB_IntParam_SolutionLimit, 1);
  const auto first = model.addVar(0.0, 1.0, 1.0, GRB_BINARY);
  const auto second = model.addVar(0.0, 1.0, 2.0, GRB_BINARY);
  model.addConstr(first + second >= 1.0);
  model.optimize();
  ASSERT_EQ(model.get(GRB_IntAttr_Status), GRB_SOLUTION_LIMIT);
  ASSERT_GT(model.get(GRB_IntAttr_SolCount), 0);
  EXPECT_GE(first.get(GRB_DoubleAttr_X) + second.get(GRB_DoubleAttr_X), 1.0);
  EXPECT_EQ(mapper::ClassifyILPResult(model.get(GRB_IntAttr_Status),
                                      model.get(GRB_IntAttr_SolCount), true),
            mapper::MappingStatus::kOptimalityUnknown);
  EXPECT_EQ(mapper::ClassifyILPResult(model.get(GRB_IntAttr_Status),
                                      model.get(GRB_IntAttr_SolCount), false),
            mapper::MappingStatus::kFailure);
}
