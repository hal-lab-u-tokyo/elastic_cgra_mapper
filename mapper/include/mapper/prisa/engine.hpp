#pragma once

#include <vector>

namespace mapper::prisa {
class PRISAEngine {
 public:
  int MaxIterations() const;
  const std::vector<int>& Placement2DResourceOrder() const;
  const std::vector<int>& ResourceOrderIndex() const;
  int ResourceSideLength() const;
  int WeakPairCount(int resource_count) const;
  void BuildDistanceRegions();
};
}  // namespace mapper::prisa
