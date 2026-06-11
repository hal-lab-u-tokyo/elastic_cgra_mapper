#include <gtest/gtest.h>

#include <cpp_simulator/CGRA.hpp>
#include <io/architecture_io.hpp>
#include <io/dfg_io.hpp>
#include <mapper/gurobi_placement_ilp_mapper.hpp>

TEST(SimulatorTest, simulator_default_result_test) {
  std::shared_ptr<entity::DFG> dfg_ptr = std::make_shared<entity::DFG>();
  std::shared_ptr<entity::MRRG> mrrg_ptr = std::make_shared<entity::MRRG>();

  entity::DFGConfig dfg_config;
  *dfg_ptr = io::ReadDFGDotFile(
      "../../../../simulator/cpp/test/data/matrixmultiply.dot", dfg_config);
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

  mapper::IMapper* mapper;
  mapper = mapper::GurobiPlacementILPMapper().CreateMapper(dfg_ptr, mrrg_ptr);
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

  // Gurobi version/search order can shift a valid output latency, so verify
  // the expected sequence instead of relying on a fixed result offset.
  std::vector<int> expected_result_vec;
  int count = 0;
  for (int i = 1; i <= 5; i++) {
    count += i * i * 2;
    expected_result_vec.push_back(count);
  }

  bool has_expected_sequence = false;
  for (std::size_t i = 0; i + expected_result_vec.size() <= result_vec.size();
       i++) {
    bool is_match = true;
    for (std::size_t j = 0; j < expected_result_vec.size(); j++) {
      if (result_vec[i + j] != expected_result_vec[j]) {
        is_match = false;
        break;
      }
    }
    if (is_match) {
      has_expected_sequence = true;
      break;
    }
  }

  EXPECT_TRUE(has_expected_sequence);
}
