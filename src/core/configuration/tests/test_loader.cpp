#include <gtest/gtest.h>

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <random>
#include <sstream>
#include <string>

#include "../loader.hpp"

using namespace cppup::configuration;
namespace fs = std::filesystem;

namespace
{

fs::path make_tmp_dir(std::string_view tag)
{
  std::random_device rd;
  auto name = std::string{"cppup_loader_test_"} + std::string{tag} + "_" + std::to_string(rd());
  auto path = fs::temp_directory_path() / name;
  fs::create_directories(path);
  return path;
}

fs::path find_include_dir()
{
  if (const char* env = std::getenv("CPPUP_INCLUDE_DIR"))
  {
    return env;
  }
  // Search upwards for an `include/cppup/configuration.hpp` marker so the test
  // works whether invoked from project root, build/, or src/core/configuration/tests/.
  for (auto candidate = fs::current_path(); !candidate.empty() && candidate != candidate.root_path();
       candidate      = candidate.parent_path())
  {
    if (fs::exists(candidate / "include" / "cppup" / "configuration.hpp"))
    {
      return candidate / "include";
    }
  }
  return {};
}

fs::path compile_to_shared(const fs::path& src, const fs::path& out_dir)
{
  auto include_dir = find_include_dir();
  if (include_dir.empty()) return {};

  auto               out = out_dir / "libtest_configure.so";
  std::ostringstream cmd;
  cmd << "g++ -std=c++23 -fPIC -shared "
      << "-I" << include_dir.string() << ' ' << src.string() << " -o " << out.string()
      << " 2>/dev/null";
  if (std::system(cmd.str().c_str()) != 0) return {};
  return out;
}

void write_file(const fs::path& p, std::string_view content)
{
  std::ofstream f(p);
  f << content;
}

}  // namespace

TEST(Loader, MissingFileReturnsError)
{
  auto result = load_from_library("/nonexistent/path/to/libfoo.so");
  ASSERT_FALSE(result.has_value());
  EXPECT_FALSE(result.error().empty());
  EXPECT_NE(result.error().find("not found"), std::string::npos);
}

TEST(Loader, LibraryWithoutConfigureSymbolReturnsError)
{
  auto tmp = make_tmp_dir("no_symbol");
  auto src = tmp / "no_symbol.cpp";
  write_file(src, "int unrelated() { return 0; }\n");

  auto so = compile_to_shared(src, tmp);
  if (so.empty())
  {
    fs::remove_all(tmp);
    GTEST_SKIP() << "g++ unavailable";
  }

  auto result = load_from_library(so);
  ASSERT_FALSE(result.has_value());
  EXPECT_NE(result.error().find("Configure function not found"), std::string::npos);

  fs::remove_all(tmp);
}

TEST(Loader, LoadsConfigurationAndReturnsValue)
{
  auto tmp = make_tmp_dir("ok");
  auto src = tmp / "build_ok.cpp";
  write_file(src, R"(
#include <cppup/configuration.hpp>
using namespace cppup::configuration;
extern "C" BuildConfiguration configure() {
    BuildConfiguration c;
    c.toolchain = Toolchain{"gcc-13"};
    c.compile_flags = {Flag{"-Wall"}, Flag{"-std=c++23"}};
    c.binaries.push_back(Binary{"app", {"src/main.cpp"}});
    return c;
}
)");

  auto so = compile_to_shared(src, tmp);
  if (so.empty())
  {
    fs::remove_all(tmp);
    GTEST_SKIP() << "compile failed";
  }

  auto result = load_from_library(so);
  ASSERT_TRUE(result.has_value()) << "load_from_library should succeed";
  const auto& config = *result;
  ASSERT_TRUE(config.toolchain.has_value());
  EXPECT_EQ(config.toolchain->name, "gcc-13");
  EXPECT_EQ(config.compile_flags.size(), 2U);
  ASSERT_EQ(config.binaries.size(), 1U);
  EXPECT_EQ(config.binaries[0].name, "app");

  fs::remove_all(tmp);
}

TEST(Loader, ConfigureThrowingExceptionReturnsError)
{
  auto tmp = make_tmp_dir("throws");
  auto src = tmp / "build_throws.cpp";
  write_file(src, R"(
#include <cppup/configuration.hpp>
#include <stdexcept>
using namespace cppup::configuration;
extern "C" BuildConfiguration configure() {
    throw std::runtime_error("intentional test failure");
}
)");

  auto so = compile_to_shared(src, tmp);
  if (so.empty())
  {
    fs::remove_all(tmp);
    GTEST_SKIP() << "compile failed";
  }

  auto result = load_from_library(so);
  ASSERT_FALSE(result.has_value());
  EXPECT_NE(result.error().find("Exception"), std::string::npos);
  EXPECT_NE(result.error().find("intentional test failure"), std::string::npos);

  fs::remove_all(tmp);
}
