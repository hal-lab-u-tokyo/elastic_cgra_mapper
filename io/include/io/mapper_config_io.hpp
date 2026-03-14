#pragma once

#include <entity/mapper_config.hpp>

namespace io {
entity::MapperConfig ReadMapperConfigFromJsonFile(std::string file_name);
void WriteMapperConfigToJsonFile(std::string file_name,
                                 const entity::MapperConfig& mapper_config);
}  // namespace io
