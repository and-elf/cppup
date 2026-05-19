#include <gtest/gtest.h>

#include <algorithm>

#include "../runtime.hpp"

using namespace cppup::configuration;

TEST(Runtime, HasFeatureDetection)
{
  BuildConfiguration config;
  config.features.insert("openssl");
  config.features.insert("threading");
  config.features.insert("networking");

  EXPECT_TRUE(has_feature(config, "openssl"));
  EXPECT_TRUE(has_feature(config, "threading"));
  EXPECT_TRUE(has_feature(config, "networking"));
  EXPECT_FALSE(has_feature(config, "nonexistent"));
}

TEST(Runtime, GetEnvAndGetEnvOr)
{
  BuildConfiguration config;
  config.environment["DEBUG"]     = "true";
  config.environment["PATH"]      = "/usr/bin:/bin";
  config.environment["EMPTY_VAR"] = "";

  auto debug_val = get_env(config, "DEBUG");
  ASSERT_TRUE(debug_val.has_value());
  EXPECT_EQ(debug_val.value(), "true");

  auto path_val = get_env(config, "PATH");
  ASSERT_TRUE(path_val.has_value());
  EXPECT_EQ(path_val.value(), "/usr/bin:/bin");

  auto empty_val = get_env(config, "EMPTY_VAR");
  ASSERT_TRUE(empty_val.has_value());
  EXPECT_EQ(empty_val.value(), "");

  auto nonexistent_val = get_env(config, "NONEXISTENT");
  EXPECT_FALSE(nonexistent_val.has_value());

  EXPECT_EQ(get_env_or(config, "DEBUG", "false"), "true");
  EXPECT_EQ(get_env_or(config, "NONEXISTENT", "default"), "default");
  EXPECT_EQ(get_env_or(config, "EMPTY_VAR", "default"), "");
}

TEST(Runtime, WhenFeatureConditional)
{
  BuildConfiguration config;
  config.features.insert("openssl");
  config.features.insert("threading");

  bool openssl_executed     = false;
  bool threading_executed   = false;
  bool nonexistent_executed = false;

  when_feature(config, "openssl", [&]() { openssl_executed = true; });
  when_feature(config, "threading", [&]() { threading_executed = true; });
  when_feature(config, "nonexistent", [&]() { nonexistent_executed = true; });

  EXPECT_TRUE(openssl_executed);
  EXPECT_TRUE(threading_executed);
  EXPECT_FALSE(nonexistent_executed);
}

TEST(Runtime, WhenEnvConditional)
{
  BuildConfiguration config;
  config.environment["DEBUG"] = "true";
  config.environment["MODE"]  = "release";
  config.environment["EMPTY"] = "";

  bool debug_true_executed   = false;
  bool debug_false_executed  = false;
  bool mode_release_executed = false;
  bool mode_debug_executed   = false;
  bool empty_executed        = false;
  bool nonexistent_executed  = false;

  when_env(config, "DEBUG", "true", [&]() { debug_true_executed = true; });
  when_env(config, "DEBUG", "false", [&]() { debug_false_executed = true; });
  when_env(config, "MODE", "release", [&]() { mode_release_executed = true; });
  when_env(config, "MODE", "debug", [&]() { mode_debug_executed = true; });
  when_env(config, "EMPTY", "", [&]() { empty_executed = true; });
  when_env(config, "NONEXISTENT", "value", [&]() { nonexistent_executed = true; });

  EXPECT_TRUE(debug_true_executed);
  EXPECT_FALSE(debug_false_executed);
  EXPECT_TRUE(mode_release_executed);
  EXPECT_FALSE(mode_debug_executed);
  EXPECT_TRUE(empty_executed);
  EXPECT_FALSE(nonexistent_executed);
}

TEST(Runtime, WhenEnvExistsConditional)
{
  BuildConfiguration config;
  config.environment["DEBUG"] = "true";
  config.environment["EMPTY"] = "";

  bool debug_exists_executed       = false;
  bool empty_exists_executed       = false;
  bool nonexistent_exists_executed = false;

  when_env_exists(config, "DEBUG", [&]() { debug_exists_executed = true; });
  when_env_exists(config, "EMPTY", [&]() { empty_exists_executed = true; });
  when_env_exists(config, "NONEXISTENT", [&]() { nonexistent_exists_executed = true; });

  EXPECT_TRUE(debug_exists_executed);
  EXPECT_TRUE(empty_exists_executed);
  EXPECT_FALSE(nonexistent_exists_executed);
}

TEST(Runtime, HasAllAndAnyFeatures)
{
  BuildConfiguration config;
  config.features.insert("openssl");
  config.features.insert("threading");
  config.features.insert("networking");

  const std::vector<std::string> ot       = {"openssl", "threading"};
  const std::vector<std::string> otn      = {"openssl", "threading", "networking"};
  const std::vector<std::string> on       = {"openssl", "nonexistent"};
  const std::vector<std::string> nn       = {"nonexistent1", "nonexistent2"};
  const std::vector<std::string> empty    = {};
  const std::vector<std::string> on2      = {"openssl", "nonexistent"};
  const std::vector<std::string> nt       = {"nonexistent", "threading"};

  EXPECT_TRUE(has_all_features(config, ot));
  EXPECT_TRUE(has_all_features(config, otn));
  EXPECT_FALSE(has_all_features(config, on));
  EXPECT_FALSE(has_all_features(config, nn));
  EXPECT_TRUE(has_all_features(config, empty));

  EXPECT_TRUE(has_any_feature(config, on2));
  EXPECT_TRUE(has_any_feature(config, nt));
  EXPECT_TRUE(has_any_feature(config, otn));
  EXPECT_FALSE(has_any_feature(config, nn));
  EXPECT_FALSE(has_any_feature(config, empty));
}

TEST(Runtime, RealisticConfiguration)
{
  BuildConfiguration config;
  config.features.insert("openssl");
  config.features.insert("threading");
  config.environment["DEBUG"]        = "true";
  config.environment["OPTIMIZATION"] = "O2";
  config.environment["TARGET"]       = "production";

  std::vector<std::string> compile_flags;
  std::vector<std::string> link_flags;
  std::vector<std::string> packages;
  std::vector<std::string> definitions;

  when_feature(config, "openssl",
               [&]()
               {
                 packages.emplace_back("openssl");
                 definitions.emplace_back("HAVE_OPENSSL=1");
               });
  when_feature(config, "threading",
               [&]()
               {
                 link_flags.emplace_back("-pthread");
                 definitions.emplace_back("HAVE_THREADING=1");
               });
  when_feature(config, "nonexistent", [&]() { packages.emplace_back("should_not_be_added"); });

  when_env(config, "DEBUG", "true",
           [&]()
           {
             compile_flags.insert(compile_flags.end(), {"-g", "-O0"});
             definitions.emplace_back("DEBUG_MODE=1");
           });
  when_env(config, "OPTIMIZATION", "O2", [&]() { compile_flags.emplace_back("-O2"); });
  when_env(config, "TARGET", "development", [&]() { definitions.emplace_back("DEV_BUILD=1"); });
  when_env(config, "TARGET", "production", [&]() { definitions.emplace_back("PROD_BUILD=1"); });

  EXPECT_NE(std::find(packages.begin(), packages.end(), "openssl"), packages.end());
  EXPECT_EQ(std::find(packages.begin(), packages.end(), "should_not_be_added"), packages.end());
  EXPECT_NE(std::find(link_flags.begin(), link_flags.end(), "-pthread"), link_flags.end());
  EXPECT_NE(std::find(compile_flags.begin(), compile_flags.end(), "-g"), compile_flags.end());
  EXPECT_NE(std::find(compile_flags.begin(), compile_flags.end(), "-O0"), compile_flags.end());
  EXPECT_NE(std::find(compile_flags.begin(), compile_flags.end(), "-O2"), compile_flags.end());
  EXPECT_NE(std::find(definitions.begin(), definitions.end(), "HAVE_OPENSSL=1"), definitions.end());
  EXPECT_NE(std::find(definitions.begin(), definitions.end(), "HAVE_THREADING=1"),
            definitions.end());
  EXPECT_NE(std::find(definitions.begin(), definitions.end(), "DEBUG_MODE=1"), definitions.end());
  EXPECT_NE(std::find(definitions.begin(), definitions.end(), "PROD_BUILD=1"), definitions.end());
  EXPECT_EQ(std::find(definitions.begin(), definitions.end(), "DEV_BUILD=1"), definitions.end());
}
