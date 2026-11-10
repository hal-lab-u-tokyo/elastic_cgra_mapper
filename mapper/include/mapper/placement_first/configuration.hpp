#pragma once

#include <mapper/configuration.hpp>
#include <optional>

namespace mapper {

struct PlacementFirstConfig : AlgorithmConfig {
  PlacementFirstConfig() { type = "PlacementFirstHeuristicMapper"; }

  const char* FamilyName() const override { return "PlacementFirst"; }
  void ReadOptions(const boost::property_tree::ptree& tree) override;
  void WriteOptions(boost::property_tree::ptree& tree) const override;

 protected:
  void ApplyOptions(IMapper& target) const override {}
};

}  // namespace mapper
