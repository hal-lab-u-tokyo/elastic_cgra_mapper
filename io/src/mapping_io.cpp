#include <boost/foreach.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <io/json_reader.hpp>
#include <io/mapping_io.hpp>

struct ConfigIdIOStruct {
  int row_id;
  int column_id;
  int context_id;

  ConfigIdIOStruct(entity::ConfigId config_id) {
    row_id = config_id.row_id;
    column_id = config_id.column_id;
    context_id = config_id.context_id;
  }

  ConfigIdIOStruct(const boost::property_tree::ptree& ptree) {
    row_id = GetValueFromPTree<int>(ptree, "row_id");
    column_id = GetValueFromPTree<int>(ptree, "column_id");
    context_id = GetValueFromPTree<int>(ptree, "context_id");
  }

  entity::ConfigId CreateConfigId() {
    return entity::ConfigId(row_id, column_id, context_id);
  }

  boost::property_tree::ptree ConvertToPTree() {
    boost::property_tree::ptree result;
    result.put("row_id", row_id);
    result.put("column_id", column_id);
    result.put("context_id", context_id);

    return result;
  }
};

struct ConfigIOStruct {
  int context_id;
  std::vector<ConfigIdIOStruct> to_config_id;
  std::string operation_type;
  std::string operation_name;
  std::vector<ConfigIdIOStruct> from_config_id;

  ConfigIOStruct(int context_id_) {
    context_id = context_id_;
    operation_type = entity::OpTypeToString(entity::OpType::NOP);
    operation_name = "";
  }
  ConfigIOStruct(entity::CGRAConfig cgra_config, int context_id_) {
    context_id = context_id_;
    operation_type = entity::OpTypeToString(cgra_config.operation_type);
    operation_name = cgra_config.operation_name;
    for (entity::ConfigId config_id : cgra_config.to_config_id_vec) {
      to_config_id.emplace_back(config_id);
    }
    for (int i = 0; i < cgra_config.from_config_id_num; i++) {
      from_config_id.emplace_back(cgra_config.from_config_id_vec[i]);
    }
  }
  ConfigIOStruct(const boost::property_tree::ptree& ptree) {
    operation_type = GetValueFromPTree<std::string>(ptree, "operation_type");
    operation_name = GetValueFromPTree<std::string>(ptree, "operation_name");
    BOOST_FOREACH (const boost::property_tree::ptree::value_type& child,
                   ptree.get_child("to_config_id")) {
      const boost::property_tree::ptree& info = child.second;

      to_config_id.emplace_back(info);
    }
    BOOST_FOREACH (const boost::property_tree::ptree::value_type& child,
                   ptree.get_child("from_config_id")) {
      const boost::property_tree::ptree& info = child.second;

      from_config_id.emplace_back(info);
    }
  }

  entity::CGRAConfig CreateCGRAConfig() {
    entity::CGRAConfig cgra_config;

    auto op = entity::OpTypeFromString(operation_type);
    for (ConfigIdIOStruct from_config_id_ele : from_config_id) {
      cgra_config.AddFromConfig(from_config_id_ele.CreateConfigId(), op,
                                operation_name);
    }
    for (ConfigIdIOStruct to_config_id_ele : to_config_id) {
      cgra_config.AddToConfig(to_config_id_ele.CreateConfigId(), op,
                              operation_name);
    }

    return cgra_config;
  }

  boost::property_tree::ptree ConvertToPTree() {
    boost::property_tree::ptree result;
    boost::property_tree::ptree to_config_id_vec_ptree,
        from_config_id_vec_ptree;

    for (ConfigIdIOStruct to_config_id : to_config_id) {
      auto new_ele = std::make_pair("", to_config_id.ConvertToPTree());
      to_config_id_vec_ptree.push_back(new_ele);
    }
    for (ConfigIdIOStruct from_config_id : from_config_id) {
      auto new_ele = std::make_pair("", from_config_id.ConvertToPTree());
      from_config_id_vec_ptree.push_back(new_ele);
    }
    result.put("context_id", context_id);
    result.put("operation_type", operation_type);
    result.put("operation_name", operation_name);
    result.add_child("to_config_id", to_config_id_vec_ptree);
    result.add_child("from_config_id", from_config_id_vec_ptree);

    return result;
  }
};

struct PEConfigIOStruct {
  int row_id;
  int column_id;
  std::vector<ConfigIOStruct> config;

  PEConfigIOStruct(int row_id_, int column_id_)
      : row_id(row_id_), column_id(column_id_) {}

  PEConfigIOStruct(const boost::property_tree::ptree& ptree) {
    row_id = GetValueFromPTree<int>(ptree, "row_id");
    column_id = GetValueFromPTree<int>(ptree, "column_id");
    BOOST_FOREACH (const boost::property_tree::ptree::value_type& child,
                   ptree.get_child("config")) {
      const boost::property_tree::ptree& info = child.second;

      config.emplace_back(info);
    }
  }

  boost::property_tree::ptree ConvertToPTree() {
    boost::property_tree::ptree result;
    boost::property_tree::ptree config_vec_ptree;
    for (ConfigIOStruct config_ele : config) {
      auto new_ele = std::make_pair("", config_ele.ConvertToPTree());
      config_vec_ptree.push_back(new_ele);
    }

    result.put("row_id", row_id);
    result.put("column_id", column_id);
    result.add_child("config", config_vec_ptree);

    return result;
  }
};

struct MappingIOStruct {
  int column;
  int row;
  int context_size;
  std::string memory_io_type;
  std::string cgra_type;
  std::string network_type;

  std::vector<PEConfigIOStruct> PE_config;

  MappingIOStruct(entity::MRRGConfig mrrg_config,
                  std::shared_ptr<entity::Mapping> mapping_ptr_) {
    column = mrrg_config.column;
    row = mrrg_config.row;
    context_size = mrrg_config.context_size;
    memory_io_type = entity::MRRGMemoryIoTypeToString(mrrg_config.memory_io);
    cgra_type = entity::MRRGCGRATypeToString(mrrg_config.cgra_type);
    network_type = entity::MRRGNetworkTypeToString(mrrg_config.network_type);

    auto config_map = mapping_ptr_->GetConfigMap();
    for (int row_id = 0; row_id < mrrg_config.row; row_id++) {
      for (int column_id = 0; column_id < mrrg_config.column; column_id++) {
        PEConfigIOStruct PE_config_ele(row_id, column_id);

        for (int context_id = 0; context_id < mrrg_config.context_size;
             context_id++) {
          entity::ConfigId tmp_config_id(row_id, column_id, context_id);
          if (config_map.count(tmp_config_id) == 0) {
            PE_config_ele.config.emplace_back(context_id);
          } else {
            PE_config_ele.config.emplace_back(config_map[tmp_config_id],
                                              context_id);
          }
        }

        PE_config.push_back(PE_config_ele);
      }
    }
  }

  MappingIOStruct(const boost::property_tree::ptree& ptree) {
    column = GetValueFromPTree<int>(ptree, "column");
    row = GetValueFromPTree<int>(ptree, "row");
    context_size = GetValueFromPTree<int>(ptree, "context_size");
    memory_io_type = GetValueFromPTree<std::string>(ptree, "memory_io_type");
    cgra_type = GetValueFromPTree<std::string>(ptree, "cgra_type");
    network_type = GetValueFromPTree<std::string>(ptree, "network_type");

    BOOST_FOREACH (const boost::property_tree::ptree::value_type& child,
                   ptree.get_child("PE_config")) {
      const boost::property_tree::ptree& info = child.second;

      PE_config.emplace_back(info);
    }
  }

  entity::Mapping CreateMapping() {
    entity::ConfigMap config_map;
    entity::MRRGConfig mrrg_config;

    mrrg_config.column = column;
    mrrg_config.row = row;
    mrrg_config.context_size = context_size;
    mrrg_config.memory_io = entity::MRRGMemoryIOTypeFromString(memory_io_type);
    mrrg_config.cgra_type = entity::MRRGCGRATypeFromString(cgra_type);
    mrrg_config.network_type = entity::MRRGNetworkTypeFromString(network_type);

    for (PEConfigIOStruct PE_config_ele : PE_config) {
      for (ConfigIOStruct config_ele : PE_config_ele.config) {
        entity::ConfigId config_id(PE_config_ele.row_id,
                                   PE_config_ele.column_id,
                                   config_ele.context_id);
        config_map.emplace(config_id, config_ele.CreateCGRAConfig());
      }
    }

    return entity::Mapping(mrrg_config, config_map);
  }

  boost::property_tree::ptree ConvertToPTree() {
    boost::property_tree::ptree result;
    boost::property_tree::ptree mapping_config_ptree;

    for (PEConfigIOStruct PE_config_ele : PE_config) {
      auto new_ele = std::make_pair("", PE_config_ele.ConvertToPTree());
      mapping_config_ptree.push_back(new_ele);
    }

    result.put("column", column);
    result.put("row", row);
    result.put("context_size", context_size);
    result.put("memory_io_type", memory_io_type);
    result.put("cgra_type", cgra_type);
    result.put("network_type", network_type);
    result.add_child("PE_config", mapping_config_ptree);

    return result;
  }
};

entity::Mapping io::ReadMappingFile(std::string file_name) {
  boost::property_tree::ptree ptree;
  boost::property_tree::read_json(file_name, ptree);

  MappingIOStruct mapping_io(ptree);

  return mapping_io.CreateMapping();
}

void io::WriteMappingFile(std::string file_name,
                          std::shared_ptr<entity::Mapping> mapping_ptr_,
                          entity::MRRGConfig mrrg_config) {
  MappingIOStruct mapping_io(mrrg_config, mapping_ptr_);

  boost::property_tree::write_json(file_name, mapping_io.ConvertToPTree());
  return;
};