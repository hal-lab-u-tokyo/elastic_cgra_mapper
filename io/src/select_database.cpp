#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <io/architecture_io.hpp>
#include <io/json_reader.hpp>
#include <io/mapping_io.hpp>
#include <io/select_database.hpp>

entity::Database io::select_database(
    const std::filesystem::path& database_dir_path,
    const entity::MRRGConfig& mrrg_config,
    const std::filesystem::path& dfg_path) {
  // database_dir/<database_id>/cgra_*.json
  // database_dir/<database_id>/database/mapping/<mapping_id>.json
  for (const auto& database_dir :
       std::filesystem::directory_iterator(database_dir_path)) {
    if (!database_dir.is_directory()) {
      continue;
    }
    // find cgra json file and check if the cgra config matches the input mrrg
    // config

    bool is_valid_cgra = false;
    bool is_valid_dfg = false;
    for (const auto& file :
         std::filesystem::directory_iterator(database_dir.path())) {
      if (is_valid_cgra && is_valid_dfg) {
        break;
      }

      if (!file.is_regular_file()) {
        continue;
      }

      // find cgra json file and check if the cgra config matches the input mrrg
      // config
      if (file.path().extension() == ".json" &&
          file.path().filename().string().find("cgra") != std::string::npos) {
        const auto cgra_config =
            io::ReadMRRGFromJsonFile(file.path()).GetMRRGConfig();
        if (cgra_config == mrrg_config) {
          is_valid_cgra = true;
          continue;
        }
      }

      // find input summary json file and check if the dfg file path in the
      // input summary
      if (file.path().extension() == ".json" &&
          file.path().filename().string().find("input_summary") !=
              std::string::npos) {
        // read json file and find the dfg file path in the json file
        std::ifstream json_file(file.path());
        boost::property_tree::ptree ptree;
        boost::property_tree::read_json(json_file, ptree);

        const std::string input_summary_dfg_file =
            GetValueFromPTree<std::string>(ptree, "dfg_file");
        const std::filesystem::path input_summary_dfg_path(
            input_summary_dfg_file);

        if (input_summary_dfg_path.filename() == dfg_path.filename()) {
          is_valid_dfg = true;
          continue;
        }
      }
    }

    if (!is_valid_cgra || !is_valid_dfg) {
      continue;
    }

    entity::Database database;

    const auto mapping_dir_path = database_dir.path() / "database" / "mapping";
    if (!std::filesystem::exists(mapping_dir_path)) {
      continue;
    }
    for (const auto& mapping_dir :
         std::filesystem::directory_iterator(mapping_dir_path)) {
      if (!mapping_dir.is_directory()) {
        continue;
      }
      bool is_success = false;
      for (const auto& file :
           std::filesystem::directory_iterator(mapping_dir.path())) {
        if (!file.is_regular_file()) {
          continue;
        }

        if (file.path().extension() == ".json" &&
            file.path().filename().string().find("output_log") !=
                std::string::npos) {
          std::ifstream json_file(file.path());
          boost::property_tree::ptree ptree;
          boost::property_tree::read_json(json_file, ptree);
          is_success = GetValueFromPTree<bool>(ptree, "is_success");
          break;
        }
      }

      if (!is_success) {
        continue;
      }

      for (const auto& file :
           std::filesystem::directory_iterator(mapping_dir.path())) {
        if (!file.is_regular_file()) {
          continue;
        }

        if (file.path().extension() == ".json" &&
            file.path().filename().string().find("mapping") !=
                std::string::npos) {
          const auto mapping = io::ReadMappingFile(file.path());
          database.AddMapping(mapping);
        }
      }
    }

    database.SetDatabaseDirPath(database_dir.path());
    database.SetExist(true);
    return database;
  }

  return entity::Database();
}
