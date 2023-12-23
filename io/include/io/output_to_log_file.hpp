#pragma once

#include <entity/mapping.hpp>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace io {

struct MappingInput {
  std::filesystem::path dfg_dot_file_path;
  entity::MRRGConfig mrrg_config;
  std::filesystem::path output_dir_path;
  double timeout_s;
  int parallel_num;
};
struct MappingOutput {
  double mapping_time_s;
  bool is_success;
  std::shared_ptr<entity::Mapping> mapping_ptr;
  entity::MRRGConfig mrrg_config;
};

struct RemapperInput {
  std::filesystem::path mapping_dir_path;
  std::filesystem::path cgra_file_path;
  std::filesystem::path output_dir_path;
  std::string remapper_mode;
};
struct RemapperOutput {
  double remapping_time_s;
  std::shared_ptr<entity::Mapping> mapping_ptr;
  entity::MRRGConfig mrrg_config;
  int parallel_num;
};

struct CreateDatabaseInput {
  std::filesystem::path dfg_dot_file_path;
  std::filesystem::path cgra_file_path;
  std::filesystem::path output_dir_path;
  double db_timeout_s;
  double min_utilization;
};
struct CreateDatabaseOutput {
  double creating_db_time_s;
};

class Logger {
 public:
  Logger();
  std::filesystem::path GetLogFilePath() const { return log_file_path_; };

 protected:
  void InitializePath(const std::filesystem::path& output_dir_path);
  void CopyArchFile(const std::filesystem::path& original_arch_file_path);
  std::filesystem::path output_dir_path_;
  std::filesystem::path log_file_path_;
  std::filesystem::path arch_file_path_;
  std::ofstream log_file_;
  std::string log_id_;
};

class MappingLogger : public Logger {
 public:
  MappingLogger() : Logger() {}
  void LogMappingInput(const MappingInput& input);
  void LogMappingOutput(const MappingOutput& output);
  std::string GetGurobiLogFilePath() const {
    return gurobi_log_file_path_.string();
  };

 private:
  std::filesystem::path gurobi_log_file_path_;
  std::filesystem::path mapping_file_path_;
};

class RemapperLogger : public Logger {
 public:
  RemapperLogger() : Logger() {}
  void LogRemapperInput(const RemapperInput& input);
  void LogRemapperOutput(const RemapperOutput& output);
  std::string GetRemapperExecLogFilePath() const {
    return remapper_exec_log_file_path.string();
  };

 private:
  std::filesystem::path mapping_file_path_;
  std::filesystem::path remapper_exec_log_file_path;
};

class CreateDatabaseLogger : public Logger {
 public:
  CreateDatabaseLogger() : Logger() {}
  void LogCreateDatabaseInput(const CreateDatabaseInput& input);
  void LogCreateDatabaseOutput(const CreateDatabaseOutput& output);
  std::string GetNextGurobiMappingPath(double mapping_timeout_s,
                                       const entity::MRRGConfig& mrrg_config);
  std::string GetSelectionLogFilePath() const {
    return selection_log_file_path_.string();
  };
  void LogMapping(const MappingOutput& mapping_output);

 private:
  std::string database_id_;
  CreateDatabaseInput input_;
  MappingLogger mapping_logger_;
  std::filesystem::path mapping_dir_path_;
  std::filesystem::path selection_log_file_path_;
  bool start_mapping_;
};
}  // namespace io