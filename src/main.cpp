#include <io/architecture_io.hpp>
#include <io/dfg_io.hpp>
#include <io/mapping_io.hpp>
#include <iostream>
#include <mapper/gurobi_mapper.hpp>

int main() {
  std::shared_ptr<entity::DFG> dfg_ptr = std::make_shared<entity::DFG>();
  std::shared_ptr<entity::MRRG> mrrg_ptr = std::make_shared<entity::MRRG>();

  *dfg_ptr =
      io::ReadDFGDotFile("../benchmark/matrixmultiply/matrixmultiply.dot");
  *mrrg_ptr = io::ReadMRRGFromJsonFile("../data/CGRA/8x8_elastic_cgra.json");

  mapper::IILPMapper* mapper;
  mapper = mapper::GurobiILPMapper().CreateMapper(dfg_ptr, mrrg_ptr);
  std::shared_ptr<entity::Mapping> mapping_ptr =
      std::make_shared<entity::Mapping>();
  bool is_success = false;
  std::tie(is_success, *mapping_ptr) = mapper->Execution();

  if (is_success) {
    io::WriteMappingFile("../output/mapping.json", mapping_ptr,
                         mrrg_ptr->GetMRRGConfig());
  }
}