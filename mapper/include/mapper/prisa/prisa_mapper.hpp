#pragma once
#include <mapper/detail/search_mapper_base.hpp>
namespace mapper {
class PRISAMapper : public SearchMapperBase {
 public:
  PRISAMapper(const std::shared_ptr<entity::DFG> dfg,
              const std::shared_ptr<entity::MRRG> mrrg)
      : SearchMapperBase(detail::SearchKind::kAutoPRISA, dfg, mrrg) {}
};
}  // namespace mapper
