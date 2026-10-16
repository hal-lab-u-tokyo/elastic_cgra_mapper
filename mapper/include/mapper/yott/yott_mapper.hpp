#pragma once
#include <mapper/detail/search_mapper_base.hpp>
namespace mapper {
class YOTTMapper : public SearchMapperBase {
 public:
  YOTTMapper(const std::shared_ptr<entity::DFG> dfg,
              const std::shared_ptr<entity::MRRG> mrrg)
      : SearchMapperBase(detail::SearchKind::kAutoYOTT, dfg, mrrg) {}
};
}  // namespace mapper
