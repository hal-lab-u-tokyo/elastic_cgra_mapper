#include <gtest/gtest.h>

#include <boost/property_tree/json_parser.hpp>
#include <filesystem>
#include <map>
#include <mapper/ilp/configuration.hpp>
#include <mapper/io/configuration_io.hpp>
#include <mapper/mapper_factory.hpp>
#include <mapper/placement_first/configuration.hpp>
#include <mapper/prisa/configuration.hpp>
#include <mapper/sa/configuration.hpp>
#include <mapper/yoto/configuration.hpp>
#include <mapper/yott/configuration.hpp>
#include <mapper/yott_core/configuration.hpp>
#include <mapper/yott_core_repair/configuration.hpp>
#include <stdexcept>
#include <type_traits>
#include <typeindex>

namespace {
using boost::property_tree::ptree;

class RecordingMapper : public mapper::IMapper {
 public:
  std::map<std::string, std::string> values;
  mapper::MappingResult Execution() override {
    throw std::logic_error("not executed");
  }
  void SetLogFilePath(const std::string& v) override {
    values["log_file_path"] = v;
  }
  void SetTimeOut(double v) override {
    values["timeout_s"] = std::to_string(v);
  }
  void SetAcceptFeasibleSolution(bool v) override {
    values["accept_feasible_solution"] = v ? "true" : "false";
  }
  void SetPlacementOnly(bool v) override {
    values["placement_only"] = v ? "true" : "false";
  }
  void SetMaxTrials(int v) override {
    values["max_trials"] = std::to_string(v);
  }
  void SetSeedCount(int v) override {
    values["seed_count"] = std::to_string(v);
  }
  void SetRoutingRetryCount(int v) override {
    values["routing_retry_count"] = std::to_string(v);
  }
  void SetRandomSeed(int v) override {
    values["random_seed"] = std::to_string(v);
  }
  void SetMaxIterations(int v) override {
    values["max_iterations"] = std::to_string(v);
  }
  void SetElitePlacementCount(int v) override {
    values["elite_placement_count"] = std::to_string(v);
  }
  void SetIONodePolicy(const std::string& v) override {
    values["io_node_policy"] = v;
  }
  void SetTrialSeedPolicy(const std::string& v) override {
    values["trial_seed_policy"] = v;
  }
  void SetTraversalOrderPolicy(const std::string& v) override {
    values["traversal_order_policy"] = v;
  }
  void SetTraversalNeighborPolicy(const std::string& v) override {
    values["traversal_neighbor_policy"] = v;
  }
  void SetCandidateScopePolicy(const std::string& v) override {
    values["candidate_scope_policy"] = v;
  }
  void SetCandidateRankPolicy(const std::string& v) override {
    values["candidate_rank_policy"] = v;
  }
  void SetTraceTrials(bool v) override {
    values["trace_trials"] = (v ? "true" : "false");
  }
  void SetUseYOTTAnnotations(bool v) override {
    values["use_yott_annotations"] = (v ? "true" : "false");
  }
};

ptree Defaults() {
  ptree tree;
  boost::property_tree::read_json(MAPPER_DEFAULT_CONFIG_FILE, tree);
  return tree;
}
void Save(const ptree& tree, const std::string& path) {
  boost::property_tree::write_json(path, tree);
}
}  // namespace

TEST(MapperConfigTest,
     every_registered_variant_reads_writes_and_applies_consumed_options) {
  const std::set<std::string> integers = {
      "max_trials",  "seed_count",     "routing_retry_count",
      "random_seed", "max_iterations", "elite_placement_count"};
  const std::set<std::string> booleans = {
      "accept_feasible_solution", "placement_only", "use_yott_annotations",
      "trace_trials"};
  for (const auto& name : mapper::GetRegisteredMapperTypes()) {
    SCOPED_TRACE(name);
    auto expected = mapper::CreateAlgorithmConfig(name);
    ASSERT_TRUE(expected->supported_options);
    auto input = Defaults();
    input.put("Exec.type", name);
    for (const auto& key : *expected->supported_options) {
      const auto path =
          std::string("Algorithm.") + expected->FamilyName() + "." + key;
      if (integers.count(key))
        input.put(path, 0);
      else if (booleans.count(key))
        input.put(path, key == "placement_only");
      else
        input.put(path, "configured-policy");
    }
    Save(input, "variant-input.json");
    auto config = mapper::ReadMapperConfigFromJsonFile("variant-input.json");
    EXPECT_EQ(std::type_index(typeid(*config.algorithm_config)),
              std::type_index(typeid(*expected)));
    RecordingMapper target;
    config.algorithm_config->Apply(target);
    for (const auto& key : *expected->supported_options) {
      const auto value = integers.count(key) ? "0"
                         : booleans.count(key)
                             ? (key == "placement_only" ? "true" : "false")
                             : "configured-policy";
      EXPECT_EQ(target.values.at(key), value);
    }
    EXPECT_FALSE(target.values.count("log_file_path"));
    EXPECT_FALSE(target.values.count("timeout_s"));
    mapper::WriteMapperConfigToJsonFile("variant-output.json", config);
    auto repeated = mapper::ReadMapperConfigFromJsonFile("variant-output.json");
    RecordingMapper again;
    repeated.algorithm_config->Apply(again);
    EXPECT_EQ(target.values, again.values);
    EXPECT_EQ(repeated.algorithm_config->type, name);
  }
}

TEST(MapperConfigTest, default_bank_is_valid_for_every_alias) {
  for (const auto& name : mapper::GetRegisteredMapperTypes()) {
    SCOPED_TRACE(name);
    auto input = Defaults();
    ASSERT_EQ(input.get_child("Algorithm").size(), 8);
    input.put("Exec.type", name);
    Save(input, "default-input.json");
    auto config = mapper::ReadMapperConfigFromJsonFile("default-input.json");
    RecordingMapper target;
    config.algorithm_config->Apply(target);
    mapper::WriteMapperConfigToJsonFile("default-output.json", config);
    auto output = mapper::ReadMapperConfigFromJsonFile("default-output.json");
    EXPECT_EQ(output.algorithm_config->type, name);
    EXPECT_EQ(output.algorithm_defaults.size(), 8);
  }
}

TEST(MapperConfigTest, selected_edits_preserve_other_family_defaults) {
  auto input = Defaults();
  input.put("Exec.type", "SA");
  input.put("Algorithm.SA.random_seed", 0);
  input.put("Algorithm.YOTT.max_trials", 19);
  Save(input, "bank-input.json");
  auto config = mapper::ReadMapperConfigFromJsonFile("bank-input.json");
  auto* sa = dynamic_cast<mapper::SAConfig*>(config.algorithm_config.get());
  ASSERT_NE(sa, nullptr);
  EXPECT_EQ(sa->random_seed, 0);
  sa->max_iterations = 7;
  mapper::WriteMapperConfigToJsonFile("bank-output.json", config);
  auto output = mapper::ReadMapperConfigFromJsonFile("bank-output.json");
  EXPECT_EQ(dynamic_cast<mapper::SAConfig*>(output.algorithm_config.get())
                ->max_iterations,
            7);
  EXPECT_EQ(dynamic_cast<mapper::YOTTConfig*>(
                output.algorithm_defaults.at("YOTT").get())
                ->max_trials,
            19);
  ptree tree;
  boost::property_tree::read_json("bank-output.json", tree);
  EXPECT_EQ(tree.get_child("Exec").size(), 1);
}

TEST(MapperConfigTest, unused_and_wrong_variant_options_are_rejected) {
  auto input = Defaults();
  input.put("Exec.type", "SA");
  input.put("Algorithm.SA.max_trials", 10);  // Log-only, not a search budget.
  Save(input, "bad-config.json");
  EXPECT_THROW(mapper::ReadMapperConfigFromJsonFile("bad-config.json"),
               std::invalid_argument);
  input = Defaults();
  input.put("Exec.type", "PRISA");
  input.put("Algorithm.PRISA.max_trials", 10);  // CostAware variants only.
  Save(input, "bad-config.json");
  EXPECT_THROW(mapper::ReadMapperConfigFromJsonFile("bad-config.json"),
               std::invalid_argument);
  input = Defaults();
  input.put("Exec.type", "ModuloSAMapper");
  input.put("Algorithm.SA.placement_only",
            true);  // Legacy wrapper doesn't forward it.
  Save(input, "bad-config.json");
  EXPECT_THROW(mapper::ReadMapperConfigFromJsonFile("bad-config.json"),
               std::invalid_argument);
  input = Defaults();
  input.put("Exec.timeout_s",
            3.0);  // CLI argument, not persistent configuration.
  Save(input, "bad-config.json");
  EXPECT_THROW(mapper::ReadMapperConfigFromJsonFile("bad-config.json"),
               std::invalid_argument);
  input = Defaults();
  input.put("Algorithm.NotAnAlgorithm.max_trials", 1);
  Save(input, "bad-config.json");
  EXPECT_THROW(mapper::ReadMapperConfigFromJsonFile("bad-config.json"),
               std::invalid_argument);
}

TEST(MapperConfigTest, legacy_noop_options_do_not_change_saved_experiments) {
  ptree input;
  input.put("DFG.operation_name_label", "opcode");
  input.put("Algorithm.type", "SA");
  input.put("Algorithm.max_trials", 30);
  input.put("Algorithm.random_seed", 0);
  input.put("Algorithm.trace_trials", true);
  input.put("Algorithm.timeout_s", 50);
  Save(input, "legacy.json");
  auto config = mapper::ReadMapperConfigFromJsonFile("legacy.json");
  RecordingMapper target;
  config.algorithm_config->Apply(target);
  EXPECT_EQ(target.values.at("random_seed"), "0");
  EXPECT_FALSE(target.values.count("max_trials"));
  EXPECT_FALSE(target.values.count("trace_trials"));
  EXPECT_FALSE(target.values.count("timeout_s"));
  input.put("Algorithm.type", "ILPMapper");
  input.put("Algorithm.accept_feasible_solution", false);
  Save(input, "legacy.json");
  config = mapper::ReadMapperConfigFromJsonFile("legacy.json");
  EXPECT_FALSE(dynamic_cast<mapper::ILPConfig*>(config.algorithm_config.get())
                   ->accept_feasible_solution);
  EXPECT_FALSE(dynamic_cast<mapper::ILPConfig*>(
                   config.algorithm_defaults.at("ILP").get())
                   ->accept_feasible_solution);
}

TEST(MapperConfigTest, persistent_options_do_not_reset_runtime_arguments) {
  auto config = mapper::CreateAlgorithmConfig("SA");
  RecordingMapper target;
  mapper::MapperOptions runtime;
  runtime.timeout_s = 3.25;
  runtime.log_file_path = "argument.log";
  target.Configure(runtime);
  config->Apply(target);
  EXPECT_EQ(target.values.at("timeout_s"), "3.250000");
  EXPECT_EQ(target.values.at("log_file_path"), "argument.log");
}

TEST(MapperConfigTest, ownership_and_invalid_tag_are_checked) {
  static_assert(!std::is_copy_constructible_v<mapper::MapperConfig>);
  static_assert(std::is_move_constructible_v<mapper::MapperConfig>);
  mapper::MapperConfig config;
  auto* pointer = config.algorithm_config.get();
  auto moved = std::move(config);
  EXPECT_EQ(moved.algorithm_config.get(), pointer);
  EXPECT_THROW(mapper::CreateAlgorithmConfig("not-an-algorithm"),
               std::invalid_argument);
  config.algorithm_config.reset();
  EXPECT_THROW(mapper::WriteMapperConfigToJsonFile("missing.json", config),
               std::invalid_argument);
  config.algorithm_config = std::make_unique<mapper::SAConfig>();
  config.algorithm_config->type = "ILPMapper";
  EXPECT_THROW(mapper::WriteMapperConfigToJsonFile("wrong-family.json", config),
               std::invalid_argument);
}
