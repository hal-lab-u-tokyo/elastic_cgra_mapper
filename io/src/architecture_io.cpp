#include <boost/optional.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <io/architecture_io.hpp>
#include <tuple>

template <class T>
struct JsonData {
  JsonData(bool _is_succeed, T _data) {
    is_succeed = _is_succeed;
    data = _data;
  }
  bool is_success;
  T data;
};

template <class T>
JsonData<T> GetValueFromPTree(const boost::property_tree::ptree& ptree,
                              const std::string& key) {
  if (boost::optional<T> value = ptree.get_optional<T>(key)) {
    return JsonData(true, value.get());
  } else {
    T result;
    return JsonData(false, result);
  }
}

entity::MRRG io::ReadMRRGFromJsonFile(std::string file_name) {
  boost::property_tree::ptree ptree;
  boost::property_tree::read_json(file_name, ptree);

  auto column_data = GetValueFromPTree<int>(ptree, "column");
  if (!column_data.is_success) {
    assert("JSON does not contain column data");
    abort();
  }

  auto row_data = GetValueFromPTree<int>(ptree, "row");
  if (!row_data.is_success) {
    assert("JSON does not contain row data");
    abort();
  }

  auto memory_io_data = GetValueFromPTree<std::string>(ptree, "memory_io");
  if (!memory_io_data.is_success) {
    assert("JSON does not contain memory io data");
    abort();
  }

  auto cgra_type_data = GetValueFromPTree<std::string>(ptree, "CGRA_type");
  if (!cgra_type_data.is_success) {
    assert("JSON does not contain CGRA type data");
    abort();
  }

  auto network_type_data =
      GetValueFromPTree<std::string>(ptree, "network_type");
  if (!network_type_data.is_success) {
    assert("JSON does not contain network type data");
    abort();
  }

  auto local_reg_size_data = GetValueFromPTree<int>(ptree, "local_reg_size");
  if (!local_reg_size_data.is_success) {
    assert("JSON does not contain local reg. size data");
    abort();
  }

  auto context_size_data = GetValueFromPTree<int>(ptree, "context_size");
  if (!context_size_data.is_success) {
    assert("JSON does not contain context size data");
    abort();
  }

  entity::MRRGConfig mrrg_config;
  mrrg_config.column = column_data.data;
  mrrg_config.row = row_data.data;
  mrrg_config.memory_io =
      entity::MRRGMemoryIOTypeFromString(memory_io_data.data);
  mrrg_config.cgra_type = entity::MRRGCGRATypeFromString(cgra_type_data.data);
  mrrg_config.network_type =
      entity::MRRGNetworkTypeFromString(network_type_data.data);
  mrrg_config.local_reg_size = local_reg_size_data.data;
  mrrg_config.context_size = context_size_data.data;

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