#pragma once

#include <mapper/configuration.hpp>
#include <optional>

namespace mapper {

struct ILPConfig : AlgorithmConfig {
  ILPConfig() { type = "ILPMapper"; }
  bool accept_feasible_solution = true;

  const char* FamilyName() const override { return "ILP"; }
  void ReadOptions(const boost::property_tree::ptree& tree) override;
  void WriteOptions(boost::property_tree::ptree& tree) const override;

 protected:
  void ApplyOptions(IMapper& target) const override {
    target.SetAcceptFeasibleSolution(accept_feasible_solution);
  }
};

}  // namespace mapper
