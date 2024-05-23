#pragma once

#include <entity/mapping.hpp>

namespace io {
entity::Mapping ReadMappingFile(std::string file_name);
void WriteMappingFile(std::string file_name,
                      std::shared_ptr<entity::Mapping> mapping_ptr_,
                      entity::MRRGConfig mrrg_config);
}  // namespace io
