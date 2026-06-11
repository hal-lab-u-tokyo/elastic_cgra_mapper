#pragma once

#include <entity/mapping.hpp>

#include <memory>
#include <string>

namespace mapper {
struct MappingResult {
  MappingResult(bool is_success, const entity::Mapping& mapping,
                double mapping_time_s)
      : is_success(is_success),
        mapping_ptr(std::make_shared<entity::Mapping>(mapping)),
        mapping_time_s(mapping_time_s){};

  bool is_success;
  std::shared_ptr<entity::Mapping> mapping_ptr;
  double mapping_time_s;
};

class IMapper {
 public:
  virtual ~IMapper() = default;
  virtual MappingResult Execution() = 0;
  virtual void SetLogFilePath(const std::string& log_file_path) = 0;
  virtual void SetTimeOut(double time_out_s) = 0;
  virtual void SetAcceptFeasibleSolution(bool accept_feasible_solution) = 0;
};
}  // namespace mapper
