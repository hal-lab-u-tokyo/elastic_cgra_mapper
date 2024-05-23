#include <boost/optional.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <io/architecture_io.hpp>
#include <io/json_reader.hpp>
#include <tuple>

entity::MRRG io::ReadMRRGFromJsonFile(std::string file_name) {
  boost::property_tree::ptree ptree;
  boost::property_tree::read_json(file_name, ptree);

  entity::MRRGConfig mrrg_config;
  mrrg_config.column = GetValueFromPTree<int>(ptree, "column");
  mrrg_config.row = GetValueFromPTree<int>(ptree, "row");
  mrrg_config.local_reg_size = GetValueFromPTree<int>(ptree, "local_reg_size");
  mrrg_config.context_size = GetValueFromPTree<int>(ptree, "context_size");

  auto memory_io_data = GetValueFromPTree<std::string>(ptree, "memory_io");
  auto cgra_type_data = GetValueFromPTree<std::string>(ptree, "CGRA_type");
  auto network_type_data =
      GetValueFromPTree<std::string>(ptree, "network_type");

  mrrg_config.memory_io = entity::MRRGMemoryIOTypeFromString(memory_io_data);
  mrrg_config.cgra_type = entity::MRRGCGRATypeFromString(cgra_type_data);
  mrrg_config.network_type =
      entity::MRRGNetworkTypeFromString(network_type_data);

  return entity::MRRG(mrrg_config);
}

void io::WriteMRRGToJsonFile(std::string file_name,
                             std::shared_ptr<entity::MRRG> mrrg_ptr_) {
  boost::property_tree::ptree ptree;
  entity::MRRGConfig mrrg_config = mrrg_ptr_->GetMRRGConfig();

  ptree.put("column", mrrg_config.column);
  ptree.put("row", mrrg_config.row);
  ptree.put("memory_io",
            entity::MRRGMemoryIoTypeToString(mrrg_config.memory_io));
  ptree.put("CGRA_type", entity::MRRGCGRATypeToString(mrrg_config.cgra_type));
  ptree.put("network_type",
            entity::MRRGNetworkTypeToString(mrrg_config.network_type));
  ptree.put("local_reg_size", mrrg_config.local_reg_size);
  ptree.put("context_size", mrrg_config.context_size);

  boost::property_tree::write_json(file_name, ptree);
  return;
}
