#pragma once
#include <mapper/detail/search_mapper_base.hpp>
namespace mapper {
class SAMapper : public SearchMapperBase {
 public:
  SAMapper(const std::shared_ptr<entity::DFG> dfg,
              const std::shared_ptr<entity::MRRG> mrrg)
      : SearchMapperBase(detail::SearchKind::kAutoSA, dfg, mrrg) {}
};
}  // namespace mapper
