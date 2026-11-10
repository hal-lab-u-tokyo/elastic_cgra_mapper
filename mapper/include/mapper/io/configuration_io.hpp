#pragma once

#include <mapper/configuration.hpp>
#include <string>

namespace mapper {
MapperConfig ReadMapperConfigFromJsonFile(const std::string& file_name);
void WriteMapperConfigToJsonFile(const std::string& file_name,
                                 const MapperConfig& config);
}  // namespace mapper
