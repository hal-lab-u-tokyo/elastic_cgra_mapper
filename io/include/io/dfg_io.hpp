#pragma once

#include <entity/dfg.hpp>
#include <string>

namespace io {
entity::DFG ReadDFGDotFile(std::string file_name);
void WriteDFGDotFile(std::string file_name,
                     std::shared_ptr<entity::DFG> dfg_ptr_);
}  // namespace io
