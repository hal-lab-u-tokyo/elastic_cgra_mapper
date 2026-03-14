#pragma once

#include <entity/dfg.hpp>
#include <entity/mapper_config.hpp>
#include <string>

namespace io {
entity::DFG ReadDFGDotFile(std::string file_name, entity::DFGConfig dfg_config);
void WriteDFGDotFile(std::string file_name,
                     std::shared_ptr<entity::DFG> dfg_ptr_,
                     entity::DFGConfig dfg_config);
}  // namespace io
