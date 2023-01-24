#pragma once

#include <entity/mrrg.hpp>

namespace io {
entity::MRRG ReadMRRGFromJsonFile(std::string file_name);
void WriteMRRGToJsonFile(std::string file_name,
                        std::shared_ptr<entity::MRRG> mrrg_ptr_);
}  // namespace io