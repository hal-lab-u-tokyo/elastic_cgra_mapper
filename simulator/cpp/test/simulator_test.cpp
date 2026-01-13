#include <gtest/gtest.h>

#include <cpp_simulator/CGRA.hpp>
#include <io/architecture_io.hpp>
#include <io/dfg_io.hpp>
#include <mapper/gurobi_mapper.hpp>

TEST(SimulatorTest, simulator_default_result_test) {
  std::shared_ptr<entity::DFG> dfg_ptr = std::make_shared<entity::DFG>();
  std::shared_ptr<entity::MRRG> mrrg_ptr = std::make_shared<entity::MRRG>();

  *dfg_ptr = io::ReadDFGDotFile(
      "../../../../simulator/cpp/test/data/matrixmultiply.dot");
  *mrrg_ptr = io::ReadMRRGFromJsonFile(
      "../../../../simulator/cpp/test/data/4x4_default_cgra.json");

  // verify A[0][i] * A[i][0]
  // const9: 0, const7: 4, const3: 1000, const16: 1, const 18: 1, const1: 0
  for (int i = 0; i < dfg_ptr->GetNodeNum(); i++) {
    entity::DFGNodeProperty node_property = dfg_ptr->GetNodeProperty(i);
    if (node_property.op_name == "const9") {
      node_property.const_value = 0;
    } else if (node_property.op_name == "const7") {
      node_property.const_value = 4;
    } else if (node_property.op_name == "const3") {
      node_property.const_value = 1000;
    } else if (node_property.op_name == "const16") {
      node_property.const_value = 1;
    } else if (node_property.op_name == "const18") {
      node_property.const_value = 1;
    } else if (node_property.op_name == "const1") {
      node_property.const_value = 0;
    }
    dfg_ptr->SetNodeProperty(i, node_property);
  }

  mapper::IILPMapper* mapper;
  mapper = mapper::GurobiILPMapper().CreateMapper(dfg_ptr, mrrg_ptr);
  std::shared_ptr<entity::Mapping> mapping_ptr =
      std::make_shared<entity::Mapping>();
  const auto result = mapper->Execution();

  std::cout << "start cgra constructor" << std::endl;
  simulator::CGRA cgra(mrrg_ptr->GetMRRGConfig());
  cgra.SetConfig(result.mapping_ptr);

  std::cout << "setup memory data" << std::endl;
  // A[0][i] = i, A[i][0] = 2i
  for (int i = 0; i < 20; i++) {
    cgra.StoreMemoryData(i * 4, i);
    cgra.StoreMemoryData(i * 1000, i * 2);
  }

  std::cout << "start exec" << std::endl;
  for (int i = 0; i < 20; i++) {
    std::cout << "reg update" << std::endl;
    cgra.RegisterUpdate();
    std::cout << "update" << std::endl;
    cgra.Update();
  }

  std::vector<int> result_vec = cgra.GetOutputResult();

  int count = 0;
  int tmp = 1;
  for (int i = 0; i < 5; i++) {
    count += tmp * tmp * 2;
    EXPECT_EQ(result_vec[i + 4], count);
    tmp++;
  }
}
