#pragma once
#include <entity/database.hpp>
#include <entity/mrrg.hpp>
#include <filesystem>

namespace io {
struct SelectedDatabase {
  entity::Database database;
  std::filesystem::path database_dir_path;
  bool exist = false;
};

SelectedDatabase select_database(const std::filesystem::path& database_dir_path,
                                 const entity::MRRGConfig& mrrg_config,
                                 const std::filesystem::path& dfg_path);

}  // namespace io
