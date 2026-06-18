#pragma once

#include <entity/mapping.hpp>
#include <entity/mrrg.hpp>
#include <filesystem>

namespace entity {
class Database {
 public:
  Database() = default;
  void AddMapping(const Mapping& mapping) { mapping_vec_.push_back(mapping); }
  const std::vector<Mapping>& GetMappings() const { return mapping_vec_; }
  int GetMinMappingOpNum() const;
  void LimitMappingNum(int mapping_num_limit);
  void SetDatabaseDirPath(const std::filesystem::path& database_dir_path) {
    database_dir_path_ = database_dir_path;
  }
  const std::filesystem::path& GetDatabaseDirPath() const {
    return database_dir_path_;
  }
  void SetExist(bool exist) { exist_ = exist; }
  bool Exist() const { return exist_; }

 private:
  std::vector<Mapping> mapping_vec_;
  MRRGConfig mrrg_config_;
  std::filesystem::path database_dir_path_;
  bool exist_ = false;
};
}  // namespace entity
