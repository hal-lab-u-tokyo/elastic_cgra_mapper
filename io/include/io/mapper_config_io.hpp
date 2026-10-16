#pragma once

#include <mapper/configuration.hpp>

namespace io {
mapper::MapperConfig ReadMapperConfigFromJsonFile(std::string file_name);
void WriteMapperConfigToJsonFile(std::string file_name,
                                 const mapper::MapperConfig& mapper_config);
}  // namespace io
