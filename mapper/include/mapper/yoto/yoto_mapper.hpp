#pragma once
#include <mapper/detail/search_mapper_base.hpp>
namespace mapper {
class YOTOMapper : public SearchMapperBase {
 public:
  YOTOMapper(const std::shared_ptr<entity::DFG> dfg,
              const std::shared_ptr<entity::MRRG> mrrg)
      : SearchMapperBase(detail::SearchKind::kAutoYOTO, dfg, mrrg) {}
};
}  // namespace mapper
