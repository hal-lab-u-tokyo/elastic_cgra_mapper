#pragma once

#include <mapper/mapper.hpp>

namespace mapper {

class GurobiPlacementILPMapper : public IILPMapper {
 public:
  GurobiPlacementILPMapper(){};
  GurobiPlacementILPMapper(const std::shared_ptr<entity::DFG> dfg_ptr,
                           const std::shared_ptr<entity::MRRG> mmrg_ptr);
  GurobiPlacementILPMapper* CreateMapper(
      const std::shared_ptr<entity::DFG> dfg_ptr,
      const std::shared_ptr<entity::MRRG> mmrg_ptr);
  MappingResult Execution();
  void SetLogFilePath(const std::string& log_file_path);
  void SetTimeOut(double time_out_s);

 private:
  std::shared_ptr<entity::DFG> dfg_ptr_;
  std::shared_ptr<entity::MRRG> mrrg_ptr_;
  std::optional<std::string> log_file_path_;
  std::optional<double> timeout_s_;
};
}  // namespace mapper
