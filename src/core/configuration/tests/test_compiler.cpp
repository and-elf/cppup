#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <thread>

#include "../compiler.hpp"

using namespace cppup::configuration;

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
  ConfigurationCompiler const compiler;

  std::filesystem::create_directories("test_temp");
  std::filesystem::create_directories(".cppup/build/config");

  std::filesystem::path const build_cpp = "test_temp/build.cpp";
  std::ofstream(build_cpp) << "// build file";

  auto shared_lib_path = compiler.get_shared_library_path(build_cpp);

  EXPECT_TRUE(compiler.needs_recompilation(build_cpp, shared_lib_path));

  std::ofstream(shared_lib_path) << "fake shared library content";
  EXPECT_FALSE(compiler.needs_recompilation(build_cpp, shared_lib_path));

  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  std::ofstream(build_cpp, std::ios::app) << "// touched";
  EXPECT_TRUE(compiler.needs_recompilation(build_cpp, shared_lib_path));

  std::filesystem::remove_all("test_temp");
  std::filesystem::remove_all(".cppup");
}

TEST(ConfigurationCompiler, CleanRemovesArtifacts)
{
  ConfigurationCompiler compiler;
  std::filesystem::create_directories("test_temp");
  std::filesystem::create_directories(".cppup/build/config");

  std::filesystem::path build_cpp = "test_temp/build.cpp";
  std::ofstream(build_cpp) << "// build file";
  auto shared_lib_path = compiler.get_shared_library_path(build_cpp);
  std::ofstream(shared_lib_path) << "fake content";

  ASSERT_TRUE(std::filesystem::exists(shared_lib_path));
  compiler.clean(build_cpp);
  EXPECT_FALSE(std::filesystem::exists(shared_lib_path));

  std::ofstream(shared_lib_path) << "fake content";
  ASSERT_TRUE(std::filesystem::exists(shared_lib_path));

  compiler.clean();
  EXPECT_FALSE(std::filesystem::exists(".cppup/build/config"));

  std::filesystem::remove_all("test_temp");
  std::filesystem::remove_all(".cppup");
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
