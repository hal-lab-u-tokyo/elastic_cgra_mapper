#include <gtest/gtest.h>

#include <boost/property_tree/json_parser.hpp>
#include <filesystem>
#include <fstream>
#include <io/output_to_log_file.hpp>

TEST(IOTest, mapping_status_serialization_and_derived_legacy_success) {
  using mapper::MappingStatus;
  entity::MRRGConfig arch{};
  arch.row = arch.column = arch.context_size = 1;
  arch.local_reg_size = 0;
  arch.memory_io = entity::MRRGMemoryIOType::kAll;
  arch.cgra_type = entity::MRRGCGRAType::kElastic;
  arch.network_type = entity::MRRGNetworkType::kDiagonal;
  const auto dfg = std::filesystem::absolute("status-input.dot");
  std::ofstream(dfg) << "digraph G {}\n";
  for (auto status :
       {MappingStatus::kOptimal, MappingStatus::kSuboptimal,
        MappingStatus::kOptimalityUnknown, MappingStatus::kFailure}) {
    io::MappingLogger logger;
    io::MappingInput input{
        dfg, arch,
        std::filesystem::absolute("status-" +
                                  mapper::MappingStatusToString(status)),
        1.0, 1};
    logger.LogMappingInput(input);
    io::MappingOutput output;
    output.mapping_time_s = 0.125;
    output.status = status;
    output.mrrg_config = arch;
    output.mapping_ptr = std::make_shared<entity::Mapping>(arch);
    logger.LogMappingOutput(output);
    boost::property_tree::ptree tree;
    boost::property_tree::read_json(logger.GetOutputSummaryFilePath(), tree);
    EXPECT_EQ(tree.get<std::string>("mapping_status"),
              mapper::MappingStatusToString(status));
    EXPECT_EQ(tree.get<bool>("is_success"), mapper::HasMapping(status));
    EXPECT_DOUBLE_EQ(tree.get<double>("mapping_time_s"), 0.125);
    EXPECT_TRUE(std::filesystem::is_regular_file(
        tree.get<std::string>("mapping_file")));
  }
}
