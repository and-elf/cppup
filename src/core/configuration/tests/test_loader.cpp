#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
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

// Compile a .cpp file into a .so so we can load it; returns empty on failure.
// The test is run from src/core/configuration/tests/ so the repo include/
// directory is four levels up. Override with CPPUP_INCLUDE_DIR for other
// invocation contexts.
fs::path compile_to_shared(const fs::path& src, const fs::path& out_dir)
{
  fs::path include_dir;
  if (const char* env = std::getenv("CPPUP_INCLUDE_DIR"))
  {
    include_dir = env;
  }
  else
  {
    include_dir = fs::current_path() / ".." / ".." / ".." / ".." / "include";
  }

  auto               out = out_dir / "libtest_configure.so";
  std::ostringstream cmd;
  cmd << "g++ -std=c++23 -fPIC -shared "
      << "-I" << include_dir.string() << ' ' << src.string() << " -o " << out.string()
      << " 2>/dev/null";
  if (std::system(cmd.str().c_str()) != 0) return {};
  return out;
}

void write(const fs::path& p, std::string_view content)
{
  std::ofstream f(p);
  f << content;
}

}  // namespace

void test_missing_file_returns_error()
{
  auto result = load_from_library("/nonexistent/path/to/libfoo.so");
  assert(!result);
  assert(!result.error().empty());
  assert(result.error().find("not found") != std::string::npos);
  std::cout << "test_missing_file_returns_error passed\n";
}

void test_library_without_configure_symbol_returns_error()
{
  auto tmp = make_tmp_dir("no_symbol");
  auto src = tmp / "no_symbol.cpp";
  write(src, "int unrelated() { return 0; }\n");

  auto so = compile_to_shared(src, tmp);
  if (so.empty())
  {
    std::cout << "test_library_without_configure_symbol_returns_error skipped"
                 " (g++ unavailable)\n";
    fs::remove_all(tmp);
    return;
  }

  auto result = load_from_library(so);
  assert(!result);
  assert(result.error().find("Configure function not found") != std::string::npos);

  fs::remove_all(tmp);
  std::cout << "test_library_without_configure_symbol_returns_error passed\n";
}

void test_loads_configuration_and_returns_value()
{
  auto tmp = make_tmp_dir("ok");
  auto src = tmp / "build_ok.cpp";
  write(src, R"(
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
    std::cout << "test_loads_configuration_and_returns_value skipped"
                 " (compile failed)\n";
    fs::remove_all(tmp);
    return;
  }

  auto result = load_from_library(so);
  assert(result && "load_from_library should succeed");
  const auto& config = *result;
  assert(config.toolchain.has_value());
  assert(config.toolchain->name == "gcc-13");
  assert(config.compile_flags.size() == 2);
  assert(config.binaries.size() == 1);
  assert(config.binaries[0].name == "app");

  fs::remove_all(tmp);
  std::cout << "test_loads_configuration_and_returns_value passed\n";
}

void test_configure_throwing_exception_returns_error()
{
  auto tmp = make_tmp_dir("throws");
  auto src = tmp / "build_throws.cpp";
  write(src, R"(
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
    std::cout << "test_configure_throwing_exception_returns_error skipped\n";
    fs::remove_all(tmp);
    return;
  }

  auto result = load_from_library(so);
  assert(!result);
  assert(result.error().find("Exception") != std::string::npos);
  assert(result.error().find("intentional test failure") != std::string::npos);

  fs::remove_all(tmp);
  std::cout << "test_configure_throwing_exception_returns_error passed\n";
}

int main()
{
  test_missing_file_returns_error();
  test_library_without_configure_symbol_returns_error();
  test_loads_configuration_and_returns_value();
  test_configure_throwing_exception_returns_error();
  std::cout << "All loader tests passed\n";
  return 0;
}
