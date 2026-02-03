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
  // auto is_racoon_data = GetValueFromPTree<std::string>(ptree, "is_racoon");
  auto is_raccoon_data = ptree.get<std::string>("is_raccoon", "false");
  auto loop_pe_pos_data = ptree.get<std::string>("loop_pe_pos", "orig");
  auto tm_pe_pos_data = ptree.get<std::string>("tm_pe_pos", "prop");
  auto is_TM_raccoon_data = ptree.get<std::string>("is_TM_raccoon", "false");
  auto is_TM_raccoon_2_data = ptree.get<std::string>("is_TM_raccoon_2", "false");
  auto network_type_data =
      GetValueFromPTree<std::string>(ptree, "network_type");

  mrrg_config.memory_io = entity::MRRGMemoryIOTypeFromString(memory_io_data);
  mrrg_config.cgra_type = entity::MRRGCGRATypeFromString(cgra_type_data);
  mrrg_config.is_raccoon = (is_raccoon_data == "true");
  mrrg_config.loop_pe_pos = entity::MRRGLoopPEPositionFromString(loop_pe_pos_data);
  mrrg_config.tm_pe_pos = entity::MRRGTMPEPositionFromString(tm_pe_pos_data);
  mrrg_config.is_TM_raccoon = (is_TM_raccoon_data == "true");
  mrrg_config.is_TM_raccoon_2 = (is_TM_raccoon_2_data == "true");
  mrrg_config.network_type =
      entity::MRRGNetworkTypeFromString(network_type_data);

  std::cout << "Debugging MRRGConfig values:" << std::endl;
  std::cout << "column: " << mrrg_config.column << std::endl;
  std::cout << "row: " << mrrg_config.row << std::endl;
  std::cout << "local_reg_size: " << mrrg_config.local_reg_size << std::endl;
  std::cout << "context_size: " << mrrg_config.context_size << std::endl;
  std::cout << "memory_io: " << entity::MRRGMemoryIoTypeToString(mrrg_config.memory_io) << std::endl;
  std::cout << "CGRA_type: " << entity::MRRGCGRATypeToString(mrrg_config.cgra_type) << std::endl;
  std::cout << "network_type: " << entity::MRRGNetworkTypeToString(mrrg_config.network_type) << std::endl;
  std::cout << "is_raccoon: " << mrrg_config.is_raccoon << std::endl;
  std::cout << "loop_pe_pos: " << entity::MRRGLoopPEPositionToString(mrrg_config.loop_pe_pos) << std::endl;
  std::cout << "tm_pe_pos: " << entity::MRRGTMPEPositionToString(mrrg_config.tm_pe_pos) << std::endl;
  std::cout << "is_TM_raccoon: " << mrrg_config.is_TM_raccoon << std::endl;
  std::cout << "is_TM_raccoon_2: " << mrrg_config.is_TM_raccoon_2 << std::endl;

  return entity::MRRG(mrrg_config);
}

void io::WriteMRRGToJsonFile(std::string file_name,
                             std::shared_ptr<entity::MRRG> mrrg_ptr_) {
  boost::property_tree::ptree ptree;
  entity::MRRGConfig mrrg_config = mrrg_ptr_->GetMRRGConfig();
  std::cout << "Debugging MRRGConfig values:" << std::endl;
  std::cout << "column: " << mrrg_config.column << std::endl;
  std::cout << "row: " << mrrg_config.row << std::endl;
  std::cout << "local_reg_size: " << mrrg_config.local_reg_size << std::endl;
  std::cout << "context_size: " << mrrg_config.context_size << std::endl;
  std::cout << "memory_io: " << entity::MRRGMemoryIoTypeToString(mrrg_config.memory_io) << std::endl;
  std::cout << "CGRA_type: " << entity::MRRGCGRATypeToString(mrrg_config.cgra_type) << std::endl;
  std::cout << "network_type: " << entity::MRRGNetworkTypeToString(mrrg_config.network_type) << std::endl;
  std::cout << "is_raccoon: " << mrrg_config.is_raccoon << std::endl;
  std::cout << "loop_pe_pos: " << entity::MRRGLoopPEPositionToString(mrrg_config.loop_pe_pos) << std::endl;
  std::cout << "tm_pe_pos: " << entity::MRRGTMPEPositionToString(mrrg_config.tm_pe_pos) << std::endl;
  std::cout << "is_TM_raccoon: " << mrrg_config.is_TM_raccoon << std::endl;
  std::cout << "is_TM_raccoon_2: " << mrrg_config.is_TM_raccoon_2 << std::endl;

  ptree.put("column", mrrg_config.column);
  ptree.put("row", mrrg_config.row);
  ptree.put("memory_io",
            entity::MRRGMemoryIoTypeToString(mrrg_config.memory_io));
  ptree.put("CGRA_type", entity::MRRGCGRATypeToString(mrrg_config.cgra_type));
  ptree.put("network_type",
            entity::MRRGNetworkTypeToString(mrrg_config.network_type));
  ptree.put("local_reg_size", mrrg_config.local_reg_size);
  ptree.put("context_size", mrrg_config.context_size);
  ptree.put("is_raccoon", mrrg_config.is_raccoon ? "true" : "false");
  ptree.put("loop_pe_pos", entity::MRRGLoopPEPositionToString(mrrg_config.loop_pe_pos));
  ptree.put("tm_pe_pos", entity::MRRGTMPEPositionToString(mrrg_config.tm_pe_pos));
  ptree.put("is_TM_raccoon", mrrg_config.is_TM_raccoon ? "true" : "false");
  ptree.put("is_TM_raccoon_2", mrrg_config.is_TM_raccoon_2 ? "true" : "false");

  boost::property_tree::write_json(file_name, ptree);
  return;
}
