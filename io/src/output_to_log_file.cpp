#include "io/output_to_log_file.hpp"

#include "cassert"
#include "chrono"
#include "io/architecture_io.hpp"
#include "io/mapping_io.hpp"
#include "time.h"

io::Logger::Logger() {
  const auto tmp_time = std::chrono::system_clock::now();
  log_id_ = std::to_string(std::chrono::duration_cast<std::chrono::nanoseconds>(
                               tmp_time.time_since_epoch())
                               .count());
}

void io::Logger::InitializePath(const std::filesystem::path& output_dir_path) {
  assert(output_dir_path.is_absolute());
  if (!std::filesystem::exists(output_dir_path)) {
    std::filesystem::create_directories(output_dir_path);
  };
  output_dir_path_ = output_dir_path;
}

void io::Logger::CopyArchFile(
    const std::filesystem::path& original_arch_file_path) {
  assert(original_arch_file_path.is_absolute());

  std::filesystem::copy_file(original_arch_file_path, arch_file_path_,
                             std::filesystem::copy_options::overwrite_existing);
}

void io::MappingLogger::LogMappingInput(const io::MappingInput& input) {
  InitializePath(input.output_dir_path);
  assert(input.dfg_dot_file_path.is_absolute());

  log_file_path_ =
      output_dir_path_ / ("mapping/log/mapping_" + log_id_ + ".log");
  arch_file_path_ =
      output_dir_path_ / ("mapping/cgra/cgra_" + log_id_ + ".json");
  mapping_file_path_ =
      output_dir_path_ / ("mapping/mapping/mapping_" + log_id_ + ".json");
  gurobi_log_file_path_ =
      output_dir_path_ / ("mapping/gurobi_log/gurobi_" + log_id_ + ".log");

  if (!std::filesystem::exists(log_file_path_.parent_path())) {
    std::filesystem::create_directories(log_file_path_.parent_path());
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

  log_file_.open(log_file_path_, std::ios::app);
  log_file_ << "-- mapping input --" << std::endl;
  log_file_ << "dfg file: " << input.dfg_dot_file_path.string() << std::endl;
  log_file_ << "cgra file: " << arch_file_path_.string() << std::endl;
  log_file_ << "output dir: " << output_dir_path_.string() << std::endl;
  log_file_ << "timeout (s): " << input.timeout_s << std::endl;
  log_file_ << "parallel num: " << input.parallel_num << std::endl;

  std::shared_ptr<entity::MRRG> mrrg_ptr =
      std::make_shared<entity::MRRG>(input.mrrg_config);
  io::WriteMRRGToJsonFile(arch_file_path_.string(), mrrg_ptr);
}

void io::MappingLogger::LogMappingOutput(const io::MappingOutput& output) {
  io::WriteMappingFile(mapping_file_path_, output.mapping_ptr,
                       output.mrrg_config);
  log_file_ << "-- mapping output --" << std::endl;
  log_file_ << "mapping time (s): " << output.mapping_time_s << std::endl;
  log_file_ << "is success: " << output.is_success << std::endl;
  log_file_ << "mapping file: " << mapping_file_path_.string() << std::endl;
  log_file_ << "gurobi log file: " << gurobi_log_file_path_.string()
            << std::endl;
  log_file_.close();
}

void io::RemapperLogger::LogRemapperInput(const io::RemapperInput& input) {
  InitializePath(input.output_dir_path);
  assert(input.mapping_dir_path.is_absolute());
  assert(input.cgra_file_path.is_absolute());
  assert(input.remapper_mode == "dp" || input.remapper_mode == "greedy" ||
         input.remapper_mode == "full_search" ||
         input.remapper_mode == "dp_and_full_search");

  log_file_path_ = output_dir_path_ / ("remapping/" + input.remapper_mode +
                                       "/log/remapping_" + log_id_ + ".log");
  arch_file_path_ = output_dir_path_ / ("remapping/" + input.remapper_mode +
                                        "/cgra/cgra_" + log_id_ + ".json");
  mapping_file_path_ =
      output_dir_path_ / ("remapping/" + input.remapper_mode +
                          "/mapping/remapping_" + log_id_ + ".json");
  remapper_exec_log_file_path =
      output_dir_path_ / ("remapping/" + input.remapper_mode +
                          "/exec_log/exec_log_" + log_id_ + ".log");

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

  CopyArchFile(input.cgra_file_path);

  log_file_.open(log_file_path_, std::ios::app);
  log_file_ << "-- remapping input --" << std::endl;
  log_file_ << "mapping dir: " << input.mapping_dir_path.string() << std::endl;

  for (const auto& file :
       std::filesystem::directory_iterator(input.mapping_dir_path)) {
    log_file_ << ">> mapping file: " << file.path().string() << std::endl;
  }

  log_file_ << "cgra file: " << arch_file_path_.string() << std::endl;
  log_file_ << "output dir: " << output_dir_path_.string() << std::endl;
  log_file_ << "remapper mode: " << input.remapper_mode << std::endl;
  log_file_ << "timeout_s: " << input.timeout_s << std::endl;
}

void io::RemapperLogger::LogRemapperOutput(const io::RemapperOutput& output) {
  assert(output.remapping_time_s >= 0);
  io::WriteMappingFile(mapping_file_path_, output.mapping_ptr,
                       output.mrrg_config);
  log_file_ << "-- remapping output --" << std::endl;
  log_file_ << "remapping time (s): " << output.remapping_time_s << std::endl;
  log_file_ << "parallel num: " << output.parallel_num << std::endl;
  log_file_ << "mapping type num: " << output.mapping_type_num << std::endl;
  log_file_ << "mapping file: " << mapping_file_path_.string() << std::endl;
  log_file_ << "db num: " << output.db_num << std::endl;
  log_file_.close();
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
  InitializePath(input.output_dir_path);
  assert(input.dfg_dot_file_path.is_absolute());
  assert(input.cgra_file_path.is_absolute());
  input_ = input;

  log_file_path_ = output_dir_path_ / ("database/log/db_" + log_id_ + ".log");
  arch_file_path_ =
      output_dir_path_ / ("database/cgra/cgra_" + log_id_ + ".json");

  database_id_ = GetCGRAId(input.cgra_file_path) + "_" +
                 std::to_string(static_cast<int>(input.db_timeout_s));
  selection_log_file_path_ =
      output_dir_path_ /
      ("database/selection_log/selection_log_" + log_id_ + ".log");

  if (!std::filesystem::exists(log_file_path_.parent_path())) {
    std::filesystem::create_directories(log_file_path_.parent_path());
  }
  if (!std::filesystem::exists(arch_file_path_.parent_path())) {
    std::filesystem::create_directories(arch_file_path_.parent_path());
  }
  if (!std::filesystem::exists(selection_log_file_path_.parent_path())) {
    std::filesystem::create_directories(selection_log_file_path_.parent_path());
  }

  log_file_.open(log_file_path_, std::ios::app);
  log_file_ << "-- create database input --" << std::endl;
  log_file_ << "dfg file: " << input.dfg_dot_file_path.string() << std::endl;
  log_file_ << "cgra file: " << arch_file_path_.string() << std::endl;
  log_file_ << "output dir: " << output_dir_path_.string() << std::endl;
  log_file_ << "timeout (s): " << input.db_timeout_s << std::endl;
  log_file_ << "min utilization: " << input.min_utilization << std::endl;
  CopyArchFile(input.cgra_file_path);
}

void io::CreateDatabaseLogger::LogCreateDatabaseOutput(
    const io::CreateDatabaseOutput& output) {
  log_file_ << "-- create database output --" << std::endl;
  log_file_ << "creating db time (s): " << output.creating_db_time_s
            << std::endl;
  log_file_.close();
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

void io::CreateDatabaseLogger::DeleteAllMappingData() {
  std::filesystem::path database_dir_path =
      output_dir_path_ / ("database/mapping/" + database_id_);
  if (!std::filesystem::exists(database_dir_path)) {
    return;
  }

  for (const auto& file :
       std::filesystem::recursive_directory_iterator(database_dir_path)) {
    if (!std::filesystem::is_directory(file.path())) {
      std::filesystem::remove(file.path());
    }
  }
}

std::string io::CreateDatabaseLogger::GetNextGurobiMappingPath(
    double mapping_timeout_s, const entity::MRRGConfig& mrrg_config) {
  start_mapping_ = true;
  MappingInput mapping_input;
  mapping_input.mrrg_config = mrrg_config;
  mapping_input.dfg_dot_file_path = input_.dfg_dot_file_path;
  mapping_input.output_dir_path =
      output_dir_path_ / ("database/mapping/" + database_id_);
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

  log_file_ << "mapping_log_file:" << mapping_logger_.GetLogFilePath().string()
            << std::endl;

  start_mapping_ = false;
}
