#include <entity/dfg.hpp>
#include <string>

namespace io {
entity::DFG ReadDFGDotFile(std::string file_name);
void WriteDFGDotFile(std::string file_name, entity::DFG dfg);
}  // namespace io
