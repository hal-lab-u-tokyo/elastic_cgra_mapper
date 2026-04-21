#include "io/output_to_log_file.hpp"

#include "cassert"
#include "chrono"
#include "io/architecture_io.hpp"
#include "io/mapping_io.hpp"
#include "time.h"

io::Logger::Logger() {
  const auto tmp_time = std::chrono::system_clock::now();

  // get yyyy/mm/dd_hh:mm:ss
  std::time_t time_t_tmp = std::chrono::system_clock::to_time_t(tmp_time);
  std::tm* tm_tmp = std::localtime(&time_t_tmp);
  char buffer[30];
  std::strftime(buffer, sizeof(buffer), "%Y%m%d%H%M%S", tm_tmp);
  int milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
                         tmp_time.time_since_epoch())
                         .count() %
                     1000;

  log_id_ = std::string(buffer) + std::to_string(milliseconds);
  host_name_ = GetHostName();
  git_commit_id_ = GetGitCommitId();
}

std::string io::Logger::GetHostName() const {
  const char* hostname_env = std::getenv("HOSTNAME_FROM_HOST");
  std::string host_name = hostname_env ? std::string(hostname_env) : "unknown";
  return host_name;
}

std::string io::Logger::GetGitCommitId() const {
  char buffer[128];
  FILE* pipe = popen("git rev-parse --short HEAD", "r");
  if (!pipe) {
    std::cerr << "Failed to get git commit id" << std::endl;
    abort();
  }

  fgets(buffer, sizeof(buffer), pipe);
  int status = pclose(pipe);
  if (status == -1) {
    std::cerr << "Failed to close pipe for git commit id" << std::endl;
    abort();
  }
  std::string git_commit = std::string(buffer);
  git_commit.erase(git_commit.find_last_not_of(" \n\r\t") + 1);
  return git_commit;
}

void io::Logger::InitializePath(const std::filesystem::path& output_dir_path) {
  assert(output_dir_path.is_absolute());
  if (!std::filesystem::exists(output_dir_path)) {
    std::filesystem::create_directories(output_dir_path);
  };
  output_dir_path_ = output_dir_path;
}

void io::Logger::CopyFile(const std::filesystem::path& original_file_path,
                          const std::filesystem::path& dest_file_path) {
  assert(original_file_path.is_absolute());
  assert(dest_file_path.is_absolute());

  std::filesystem::copy_file(original_file_path, dest_file_path,
                             std::filesystem::copy_options::overwrite_existing);
}

void io::Logger::OutputJsonLog(
    const std::filesystem::path& json_log_file_path,
    const std::unordered_map<std::string, std::string>& json_str_vec) {
  std::ofstream json_log_file(json_log_file_path);
  json_log_file << "{" << std::endl;
  int count = 0;
  for (const auto& [key, value] : json_str_vec) {
    json_log_file << "  \"" << key << "\": " << value;
    if (count != json_str_vec.size() - 1) {
      json_log_file << ",";
    }
    json_log_file << std::endl;
    count++;
  }
  json_log_file << "}" << std::endl;
  json_log_file.close();
}

void io::MappingLogger::LogMappingInput(const io::MappingInput& input) {
  InitializePath(input.output_dir_path / "mapping" / log_id_);
  assert(input.dfg_dot_file_path.is_absolute());

  input_summary_file_path_ =
      output_dir_path_ / ("input_log_" + log_id_ + ".json");
  output_summary_file_path_ =
      output_dir_path_ / ("output_log_" + log_id_ + ".json");
  log_file_path_ = output_dir_path_ / ("log_" + log_id_ + ".log");
  arch_file_path_ = output_dir_path_ / ("cgra_" + log_id_ + ".json");
  mapping_file_path_ = output_dir_path_ / ("mapping_" + log_id_ + ".json");
  gurobi_log_file_path_ = output_dir_path_ / ("gurobi_log_" + log_id_ + ".log");

  if (!std::filesystem::exists(log_file_path_.parent_path())) {
    std::filesystem::create_directories(log_file_path_.parent_path());
  }
  if (!std::filesystem::exists(input_summary_file_path_.parent_path())) {
    std::filesystem::create_directories(input_summary_file_path_.parent_path());
  }
  if (!std::filesystem::exists(output_summary_file_path_.parent_path())) {
    std::filesystem::create_directories(
        output_summary_file_path_.parent_path());
  }
  if (!std::filesystem::exists(arch_file_path_.parent_path())) {
    std::filesystem::create_directories(arch_file_path_.parent_path());
  }
  if (!std::filesystem::exists(mapping_file_path_.parent_path())) {
    std::filesystem::create_directories(mapping_file_path_.parent_path());
  }
  if (!std::filesystem::exists(gurobi_log_file_path_.parent_path())) {
    std::filesystem::create_directories(gurobi_log_file_path_.parent_path());
  }

  std::unordered_map<std::string, std::string> json_str_vec;
  json_str_vec["dfg_file"] = "\"" + input.dfg_dot_file_path.string() + "\"";
  json_str_vec["cgra_file"] = "\"" + arch_file_path_.string() + "\"";
  json_str_vec["output_dir"] = "\"" + output_dir_path_.string() + "\"";
  json_str_vec["timeout_s"] = std::to_string(input.timeout_s);
  json_str_vec["parallel_num"] = std::to_string(input.parallel_num);
  json_str_vec["host_name"] = "\"" + host_name_ + "\"";
  json_str_vec["git_commit_id"] = "\"" + git_commit_id_ + "\"";
  OutputJsonLog(input_summary_file_path_, json_str_vec);

  CopyFile(input.dfg_dot_file_path,
           output_dir_path_ / (input.dfg_dot_file_path.filename().string()));

  std::shared_ptr<entity::MRRG> mrrg_ptr_;
  mrrg_ptr_ = std::make_shared<entity::MRRG>(input.mrrg_config);
  io::WriteMRRGToJsonFile(arch_file_path_.string(), mrrg_ptr_);

  return;
}

void io::MappingLogger::LogMappingOutput(const io::MappingOutput& output) {
  io::WriteMappingFile(mapping_file_path_, output.mapping_ptr,
                       output.mrrg_config);
  std::unordered_map<std::string, std::string> json_str_vec;
  json_str_vec["mapping_time_s"] = std::to_string(output.mapping_time_s);
  json_str_vec["is_success"] = output.is_success ? "true" : "false";
  json_str_vec["mapping_file"] = "\"" + mapping_file_path_.string() + "\"";
  json_str_vec["gurobi_log_file"] =
      "\"" + gurobi_log_file_path_.string() + "\"";
  OutputJsonLog(output_summary_file_path_, json_str_vec);

  return;
}

void io::RemapperLogger::LogRemapperInput(const io::RemapperInput& input) {
  InitializePath(input.output_dir_path / "remapping" / log_id_);
  assert(input.mapping_dir_path.is_absolute());
  assert(input.cgra_file_path.is_absolute());
  assert(input.remapper_mode == "dp" || input.remapper_mode == "greedy" ||
         input.remapper_mode == "full_search" ||
         input.remapper_mode == "dp_and_full_search");

  log_file_path_ = output_dir_path_ / ("log_" + log_id_ + ".log");
  arch_file_path_ = output_dir_path_ / ("cgra_" + log_id_ + ".json");
  mapping_file_path_ = output_dir_path_ / ("remapping_" + log_id_ + ".json");
  remapper_exec_log_file_path =
      output_dir_path_ / ("exec_log_" + log_id_ + ".log");
  input_summary_file_path_ =
      output_dir_path_ / ("input_summary_" + log_id_ + ".json");
  output_summary_file_path_ =
      output_dir_path_ / ("output_summary_" + log_id_ + ".json");
  input_mapping_dir_path_ = output_dir_path_ / ("input_mapping/");

  if (!std::filesystem::exists(log_file_path_.parent_path())) {
    std::filesystem::create_directories(log_file_path_.parent_path());
  }
  if (!std::filesystem::exists(arch_file_path_.parent_path())) {
    std::filesystem::create_directories(arch_file_path_.parent_path());
  }
  if (!std::filesystem::exists(mapping_file_path_.parent_path())) {
    std::filesystem::create_directories(mapping_file_path_.parent_path());
  }
  if (!std::filesystem::exists(remapper_exec_log_file_path.parent_path())) {
    std::filesystem::create_directories(
        remapper_exec_log_file_path.parent_path());
  }
  if (!std::filesystem::exists(input_summary_file_path_.parent_path())) {
    std::filesystem::create_directories(input_summary_file_path_.parent_path());
  }
  if (!std::filesystem::exists(output_summary_file_path_.parent_path())) {
    std::filesystem::create_directories(
        output_summary_file_path_.parent_path());
  }
  if (!std::filesystem::exists(input_mapping_dir_path_)) {
    std::filesystem::create_directories(input_mapping_dir_path_);
  }

  std::unordered_map<std::string, std::string> json_str_vec;
  json_str_vec["mapping_dir"] = "\"" + input.mapping_dir_path.string() + "\"";
  json_str_vec["cgra_file"] = "\"" + arch_file_path_.string() + "\"";
  json_str_vec["output_dir"] = "\"" + output_dir_path_.string() + "\"";
  json_str_vec["remapper_mode"] = input.remapper_mode;
  json_str_vec["timeout_s"] = std::to_string(input.timeout_s);
  json_str_vec["host_name"] = "\"" + host_name_ + "\"";
  json_str_vec["git_commit_id"] = "\"" + git_commit_id_ + "\"";

  std::string mapping_files_str = "[";
  for (const auto& file :
       std::filesystem::directory_iterator(input.mapping_dir_path)) {
    mapping_files_str += "\"" + file.path().string() + "\",";
  }
  if (!mapping_files_str.empty()) {
    mapping_files_str.pop_back();  // Remove the last comma
  }
  mapping_files_str += "]";

  json_str_vec["mapping_files"] = mapping_files_str;

  OutputJsonLog(input_summary_file_path_, json_str_vec);

  CopyFile(input.cgra_file_path, arch_file_path_);
  for (const auto& file :
       std::filesystem::directory_iterator(input.mapping_dir_path)) {
    if (file.path().extension() == ".json") {
      CopyFile(file.path(),
               input_mapping_dir_path_ / file.path().filename().string());
    }
  }

  return;
}

void io::RemapperLogger::LogRemapperOutput(const io::RemapperOutput& output) {
  assert(output.remapping_time_s >= 0);
  std::unordered_map<std::string, std::string> json_str_vec;
  json_str_vec["remapping_time_s"] = std::to_string(output.remapping_time_s);
  json_str_vec["parallel_num"] = std::to_string(output.parallel_num);
  json_str_vec["mapping_type_num"] = std::to_string(output.mapping_type_num);
  json_str_vec["mapping_file"] = "\"" + mapping_file_path_.string() + "\"";
  OutputJsonLog(output_summary_file_path_, json_str_vec);
}

std::string GetCGRAId(const std::filesystem::path& cgra_file_path) {
  const auto mrrg_config =
      io::ReadMRRGFromJsonFile(cgra_file_path).GetMRRGConfig();

  std::string cgra_id = "";
  cgra_id += std::to_string(mrrg_config.column);
  cgra_id += "_" + std::to_string(mrrg_config.row);
  cgra_id += "_" + std::to_string(mrrg_config.context_size);
  cgra_id += "_" + entity::MRRGMemoryIoTypeToString(mrrg_config.memory_io);
  cgra_id += "_" + entity::MRRGCGRATypeToString(mrrg_config.cgra_type);
  cgra_id += "_" + entity::MRRGNetworkTypeToString(mrrg_config.network_type);
  cgra_id += "_" + std::to_string(mrrg_config.local_reg_size);

  return cgra_id;
}

void io::CreateDatabaseLogger::LogCreateDatabaseInput(
    const io::CreateDatabaseInput& input) {
  InitializePath(input.output_dir_path / "database" / log_id_);
  assert(input.dfg_dot_file_path.is_absolute());
  assert(input.cgra_file_path.is_absolute());
  input_ = input;

  log_file_path_ = output_dir_path_ / ("log_ " + log_id_ + ".log");
  arch_file_path_ = output_dir_path_ / ("cgra_" + log_id_ + ".json");

  database_id_ = GetCGRAId(input.cgra_file_path) + "_" +
                 std::to_string(static_cast<int>(input.db_timeout_s));
  selection_log_file_path_ =
      output_dir_path_ / ("selection_log_" + log_id_ + ".log");
  input_summary_file_path_ =
      output_dir_path_ / ("input_summary_" + log_id_ + ".json");
  output_summary_file_path_ =
      output_dir_path_ / ("output_summary_" + log_id_ + ".json");

  if (!std::filesystem::exists(log_file_path_.parent_path())) {
    std::filesystem::create_directories(log_file_path_.parent_path());
  }
  if (!std::filesystem::exists(arch_file_path_.parent_path())) {
    std::filesystem::create_directories(arch_file_path_.parent_path());
  }
  if (!std::filesystem::exists(selection_log_file_path_.parent_path())) {
    std::filesystem::create_directories(selection_log_file_path_.parent_path());
  }
  if (!std::filesystem::exists(input_summary_file_path_.parent_path())) {
    std::filesystem::create_directories(input_summary_file_path_.parent_path());
  }
  if (!std::filesystem::exists(output_summary_file_path_.parent_path())) {
    std::filesystem::create_directories(
        output_summary_file_path_.parent_path());
  }

  std::unordered_map<std::string, std::string> json_str_vec;
  json_str_vec["dfg_file"] = "\"" + input.dfg_dot_file_path.string() + "\"";
  json_str_vec["cgra_file"] = "\"" + arch_file_path_.string() + "\"";
  json_str_vec["output_dir"] = "\"" + output_dir_path_.string() + "\"";
  json_str_vec["db_timeout_s"] = std::to_string(input.db_timeout_s);
  json_str_vec["min_utilization"] = std::to_string(input.min_utilization);
  json_str_vec["host_name"] = "\"" + host_name_ + "\"";
  json_str_vec["git_commit_id"] = "\"" + git_commit_id_ + "\"";
  OutputJsonLog(input_summary_file_path_, json_str_vec);

  CopyFile(input.cgra_file_path, arch_file_path_);

  return;
}

void io::CreateDatabaseLogger::LogCreateDatabaseOutput(
    const io::CreateDatabaseOutput& output) {
  std::unordered_map<std::string, std::string> json_str_vec;

  std::string mapping_output_summary_files_str = "[";
  for (size_t i = 0; i < mapping_output_summary_file_path_vec_.size(); i++) {
    mapping_output_summary_files_str +=
        "\"" + mapping_output_summary_file_path_vec_[i].string() + "\",";
    if (i == mapping_output_summary_file_path_vec_.size() - 1) {
      mapping_output_summary_files_str.pop_back();  // Remove the last comma
    }
  }
  mapping_output_summary_files_str += "]";

  json_str_vec["mapping_output_summary_files"] =
      mapping_output_summary_files_str;
  json_str_vec["creating_db_time_s"] =
      std::to_string(output.creating_db_time_s);
  OutputJsonLog(output_summary_file_path_, json_str_vec);

  return;
}

int io::CreateDatabaseLogger::countMappingDataNum() const {
  int count = 0;
  std::filesystem::path mapping_dir_path =
      output_dir_path_ /
      ("database/mapping/" + database_id_ + "/mapping/mapping");
  if (!std::filesystem::exists(mapping_dir_path)) {
    return 0;
  }

  for (const auto& file :
       std::filesystem::directory_iterator(mapping_dir_path)) {
    if (file.path().extension() == ".json") {
      count++;
    }
  }
  return count;
}

std::string io::CreateDatabaseLogger::GetNextMappingPath(
    double mapping_timeout_s, const entity::MRRGConfig& mrrg_config) {
  start_mapping_ = true;
  MappingInput mapping_input;
  mapping_input.mrrg_config = mrrg_config;
  mapping_input.dfg_dot_file_path = input_.dfg_dot_file_path;
  mapping_input.output_dir_path = output_dir_path_ / "database";
  mapping_input.timeout_s = mapping_timeout_s;
  mapping_input.parallel_num = 1;
  mapping_logger_ = MappingLogger();
  mapping_logger_.LogMappingInput(mapping_input);

  return mapping_logger_.GetGurobiLogFilePath();
}

void io::CreateDatabaseLogger::LogMapping(
    const io::MappingOutput& mapping_output) {
  assert(start_mapping_);

  mapping_logger_.LogMappingOutput(mapping_output);

  mapping_output_summary_file_path_vec_.push_back(
      mapping_logger_.GetOutputSummaryFilePath());

  start_mapping_ = false;
}
