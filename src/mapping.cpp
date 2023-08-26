#include <time.h>

#include <io/architecture_io.hpp>
#include <io/dfg_io.hpp>
#include <io/mapping_io.hpp>
#include <iostream>
#include <mapper/gurobi_mapper.hpp>

int main(int argc, char* argv[]) {
  if (argc != 5) {
    std::cerr << "invalid arguments" << std::endl;
    abort();
  }

  std::string dfg_dot_file_path = argv[1];
  std::string mrrg_file_path = argv[2];
  std::string output_mapping_path = argv[3];
  std::string log_file_dir = argv[4];

  std::shared_ptr<entity::DFG> dfg_ptr = std::make_shared<entity::DFG>();
  std::shared_ptr<entity::MRRG> mrrg_ptr = std::make_shared<entity::MRRG>();

  *dfg_ptr = io::ReadDFGDotFile(dfg_dot_file_path);
  *mrrg_ptr = io::ReadMRRGFromJsonFile(mrrg_file_path);

  mapper::GurobiILPMapper* mapper;
  mapper = mapper::GurobiILPMapper().CreateMapper(dfg_ptr, mrrg_ptr);
  mapper->SetLogFilePath(log_file_dir);

  std::shared_ptr<entity::Mapping> mapping_ptr =
      std::make_shared<entity::Mapping>();
  bool is_success = false;
  std::tie(is_success, *mapping_ptr) = mapper->Execution();

  io::WriteMappingFile(output_mapping_path, mapping_ptr,
                       mrrg_ptr->GetMRRGConfig());

}