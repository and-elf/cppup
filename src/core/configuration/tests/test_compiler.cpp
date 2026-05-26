#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>
#include <string_view>
#include <thread>

#include "../compiler.hpp"

namespace fs = std::filesystem;
using namespace cppup::configuration;

namespace
{

// Per-test scratch directory under temp_directory_path(). Older versions of
// these tests created `.cppup/build/config` under CWD and then `remove_all(".cppup")`
// at teardown, which wiped the real `.cppup/cache/build_cache.db` whenever the
// suite was run via `cppup test` from the project root. Keep all FS writes inside
// the returned directory.
fs::path make_tmp_root(std::string_view tag)
{
  std::random_device rd;
  auto               path = fs::temp_directory_path() /
              (std::string{"cppup_test_compiler_"} + std::string{tag} + "_" + std::to_string(rd()));
  fs::create_directories(path);
  return path;
}

CompilerOptions options_with_output(const fs::path& output_dir)
{
  CompilerOptions opts;
  opts.output_directory = output_dir.string();
  return opts;
}

}  // namespace

TEST(CompilerOptions, Defaults)
{
  CompilerOptions const options;
  EXPECT_EQ(options.compiler, "g++");
  EXPECT_EQ(options.cpp_standard, "c++23");
  EXPECT_FALSE(options.compile_flags.empty());
  EXPECT_FALSE(options.link_flags.empty());
  EXPECT_EQ(options.output_directory, ".cppup/build/config");
  EXPECT_FALSE(options.debug_symbols);
  EXPECT_FALSE(options.verbose);
}

TEST(ConfigurationCompiler, SharedLibraryPathDiffersByInput)
{
  ConfigurationCompiler const compiler;
  auto                        path1 = compiler.get_shared_library_path("build.cpp");
  auto                        path2 = compiler.get_shared_library_path("src/module/build.cpp");
  EXPECT_NE(path1, path2);
  EXPECT_EQ(path1.parent_path(), ".cppup/build/config");
  EXPECT_EQ(path2.parent_path(), ".cppup/build/config");

#ifdef _WIN32
  EXPECT_EQ(path1.extension(), ".dll");
  EXPECT_EQ(path2.extension(), ".dll");
#else
  EXPECT_EQ(path1.extension(), ".so");
  EXPECT_EQ(path2.extension(), ".so");
  EXPECT_TRUE(path1.filename().string().starts_with("lib"));
  EXPECT_TRUE(path2.filename().string().starts_with("lib"));
#endif
}

TEST(ConfigurationCompiler, NeedsRecompilationDetectsStaleness)
{
  const auto tmp        = make_tmp_root("needs_recompilation");
  const auto output_dir = tmp / "build_config";
  fs::create_directories(output_dir);

  ConfigurationCompiler compiler(options_with_output(output_dir));

  const fs::path build_cpp = tmp / "build.cpp";
  std::ofstream(build_cpp) << "// build file";

  const auto shared_lib_path = compiler.get_shared_library_path(build_cpp);

  EXPECT_TRUE(compiler.needs_recompilation(build_cpp, shared_lib_path));

  std::ofstream(shared_lib_path) << "fake shared library content";
  EXPECT_FALSE(compiler.needs_recompilation(build_cpp, shared_lib_path));

  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  std::ofstream(build_cpp, std::ios::app) << "// touched";
  EXPECT_TRUE(compiler.needs_recompilation(build_cpp, shared_lib_path));

  fs::remove_all(tmp);
}

TEST(ConfigurationCompiler, CleanRemovesArtifacts)
{
  const auto tmp        = make_tmp_root("clean_removes_artifacts");
  const auto output_dir = tmp / "build_config";
  fs::create_directories(output_dir);

  ConfigurationCompiler compiler(options_with_output(output_dir));

  const fs::path build_cpp = tmp / "build.cpp";
  std::ofstream(build_cpp) << "// build file";
  const auto shared_lib_path = compiler.get_shared_library_path(build_cpp);
  std::ofstream(shared_lib_path) << "fake content";

  ASSERT_TRUE(fs::exists(shared_lib_path));
  compiler.clean(build_cpp);
  EXPECT_FALSE(fs::exists(shared_lib_path));

  std::ofstream(shared_lib_path) << "fake content";
  ASSERT_TRUE(fs::exists(shared_lib_path));

  compiler.clean();
  EXPECT_FALSE(fs::exists(output_dir));

  fs::remove_all(tmp);
}

TEST(CompilationResult, DefaultIsFailure)
{
  CompilationResult result;
  EXPECT_FALSE(result.is_success());
  EXPECT_TRUE(result.is_failure());
  EXPECT_TRUE(result.shared_library_path.empty());
  EXPECT_TRUE(result.error_message.empty());
  EXPECT_TRUE(result.compiler_output.empty());
  EXPECT_EQ(result.exit_code, 0);

  result.success             = true;
  result.shared_library_path = "test.so";
  EXPECT_TRUE(result.is_success());
}
