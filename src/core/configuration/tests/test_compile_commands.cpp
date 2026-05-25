#include <gtest/gtest.h>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <random>
#include <sstream>
#include <string>

#include "../build_configuration.hpp"
#include "../compile_commands.hpp"

using namespace cppup::configuration;
namespace fs = std::filesystem;

namespace
{

fs::path make_tmp_dir(std::string_view tag)
{
  std::random_device rd;
  auto name = std::string{"cppup_cc_test_"} + std::string{tag} + "_" + std::to_string(rd());
  auto path = fs::temp_directory_path() / name;
  fs::create_directories(path);
  return path;
}

std::string slurp(const fs::path& p)
{
  std::ifstream const f(p);
  std::ostringstream  ss;
  ss << f.rdbuf();
  return ss.str();
}

std::size_t count_entries(const std::string& json)
{
  std::size_t n   = 0;
  std::size_t pos = 0;
  while ((pos = json.find("\"file\":", pos)) != std::string::npos)
  {
    ++n;
    ++pos;
  }
  return n;
}

bool contains(const std::string& s, std::string_view needle)
{
  return s.find(needle) != std::string::npos;
}

}  // namespace

TEST(CompileCommands, OneEntryPerSource)
{
  auto               root = make_tmp_dir("one_per_src");
  BuildConfiguration config;
  BuildOptions       options;
  config.binaries.push_back(Binary{.name = "app", .sources = {"src/main.cpp", "src/util.cpp"}});

  auto result = emit_compile_commands(config, root, root / "build", options);
  EXPECT_TRUE(fs::exists(result));
  EXPECT_EQ(result.filename(), "compile_commands.json");

  auto json = slurp(result);
  EXPECT_EQ(count_entries(json), 2U);
  EXPECT_TRUE(contains(json, "src/main.cpp"));
  EXPECT_TRUE(contains(json, "src/util.cpp"));
  EXPECT_TRUE(contains(json, "\"directory\": \"" + root.string() + "\""));
  EXPECT_TRUE(contains(json, "\"arguments\":"));

  fs::remove_all(root);
}

TEST(CompileCommands, IncludesDefinesAndFlags)
{
  auto               root = make_tmp_dir("flags");
  BuildOptions       options;
  BuildConfiguration config;
  config.compile_flags = {Flag{"-Wall"}, Flag{"-std=c++23"}};
  config.definitions   = {Definition{.name = "FOO", .value = "bar"}, Definition{.name = "NDEBUG"}};
  config.include_paths = {"include", "src"};
  config.binaries.push_back(Binary{.name = "app", .sources = {"src/main.cpp"}});

  auto json = slurp(emit_compile_commands(config, root, root / "build", options));
  EXPECT_TRUE(contains(json, "\"-Wall\""));
  EXPECT_TRUE(contains(json, "\"-std=c++23\""));
  EXPECT_TRUE(contains(json, "\"-DFOO=bar\""));
  EXPECT_TRUE(contains(json, "\"-DNDEBUG\""));
  EXPECT_TRUE(contains(json, "\"-I" + (root / "include").string() + "\""));
  EXPECT_TRUE(contains(json, "\"-I" + (root / "src").string() + "\""));

  fs::remove_all(root);
}

TEST(CompileCommands, ToolchainCompilerUsed)
{
  auto               root = make_tmp_dir("toolchain");
  BuildConfiguration config;
  config.toolchain = Toolchain{.name = "clang++"};
  config.binaries.push_back(Binary{.name = "app", .sources = {"main.cpp"}});
  BuildOptions options{};

  auto json = slurp(emit_compile_commands(config, root, root / "build", options));
  EXPECT_TRUE(contains(json, "\"clang++\""));

  BuildConfiguration default_config;
  default_config.binaries.push_back(Binary{.name = "app", .sources = {"main.cpp"}});
  auto default_root = make_tmp_dir("toolchain_default");
  auto json2 =
      slurp(emit_compile_commands(default_config, default_root, default_root / "build", options));
  EXPECT_TRUE(contains(json2, "\"g++\""));

  fs::remove_all(root);
  fs::remove_all(default_root);
}

TEST(CompileCommands, AllTargetKindsEmitted)
{
  auto               root = make_tmp_dir("targets");
  BuildConfiguration config;
  BuildOptions       options;
  config.libraries.push_back(Library{.name = "lib", .sources = {"lib/a.cpp", "lib/b.cpp"}});
  config.binaries.push_back(Binary{.name = "app", .sources = {"src/main.cpp"}});
  config.tests.push_back(cppup::configuration::Test{.name = "unit", .sources = {"tests/t.cpp"}});

  auto json = slurp(emit_compile_commands(config, root, root / "build", options));
  EXPECT_EQ(count_entries(json), 4U);
  EXPECT_TRUE(contains(json, "lib/a.cpp"));
  EXPECT_TRUE(contains(json, "lib/b.cpp"));
  EXPECT_TRUE(contains(json, "src/main.cpp"));
  EXPECT_TRUE(contains(json, "tests/t.cpp"));

  fs::remove_all(root);
}

TEST(CompileCommands, AsanAddsFsanitize)
{
  auto               root = make_tmp_dir("asan");
  BuildConfiguration config;
  config.binaries.push_back(Binary{.name = "app", .sources = {"main.cpp"}});

  EXPECT_FALSE(
      contains(slurp(emit_compile_commands(config, root, root / "build")), "-fsanitize=address"));

  auto with =
      slurp(emit_compile_commands(config, root, root / "build", BuildOptions{.asan = Asan::On}));
  EXPECT_TRUE(contains(with, "-fsanitize=address"));
  EXPECT_TRUE(contains(with, "-fno-omit-frame-pointer"));

  fs::remove_all(root);
}

TEST(CompileCommands, CoverageAddsCoverageFlag)
{
  auto               root = make_tmp_dir("coverage");
  BuildConfiguration config;
  BuildOptions       options{.coverage = Coverage::Off};
  config.binaries.push_back(Binary{.name = "app", .sources = {"main.cpp"}});

  EXPECT_FALSE(
      contains(slurp(emit_compile_commands(config, root, root / "build", options)), "--coverage"));

  options.coverage = Coverage::On;
  EXPECT_TRUE(
      contains(slurp(emit_compile_commands(config, root, root / "build", options)), "--coverage"));

  fs::remove_all(root);
}

TEST(CompileCommands, PathsAreAbsolute)
{
  auto               root = make_tmp_dir("abs");
  BuildConfiguration config;
  config.binaries.push_back(Binary{.name = "app", .sources = {"src/main.cpp"}});
  BuildOptions options{};

  auto json     = slurp(emit_compile_commands(config, root, root / "build", options));
  auto expected = (root / "src" / "main.cpp").string();
  EXPECT_TRUE(contains(json, "\"file\": \"" + expected + "\""));

  fs::remove_all(root);
}

TEST(CompileCommands, QuotesInDefinitionValueAreEscaped)
{
  auto               root = make_tmp_dir("escape");
  BuildConfiguration config;
  config.definitions = {Definition{.name = "VERSION", .value = "\"1.2\""}};
  config.binaries.push_back(Binary{.name = "app", .sources = {"main.cpp"}});
  BuildOptions options{};

  auto json = slurp(emit_compile_commands(config, root, root / "build", options));
  EXPECT_TRUE(contains(json, "\\\"1.2\\\""));

  fs::remove_all(root);
}
