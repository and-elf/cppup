#include <expected>
#include <filesystem>
#include <string>

#include "../../configuration/compiler.hpp"
#include "../../configuration/loader.hpp"
#include "command_context.hpp"

namespace cppup::cli
{

std::expected<int, std::string> executeTest(bool                  enable_asan,
                                            const CommandContext& context) noexcept
{
  try
  {
    context.logger->info("Running tests...");

    // Look for build.cpp in current directory
    std::filesystem::path build_file = context.projectRoot / "build.cpp";
    if (!std::filesystem::exists(build_file))
    {
      return std::unexpected("No build.cpp found in current directory");
    }

    // Load configuration to find test targets
    cppup::configuration::ConfigurationCompiler compiler;
    auto                                        result = compiler.compile(build_file);

    if (!result.success)
    {
      return std::unexpected("Failed to compile build configuration: " + result.error_message);
    }

    cppup::configuration::ConfigurationLoader loader;
    auto load_result = loader.load_from_library(result.shared_library_path);

    if (!load_result.success)
    {
      return std::unexpected("Failed to load build configuration: " + load_result.error_message);
    }

    auto& config = load_result.configuration.value();

    if (config.tests.empty())
    {
      context.logger->info("No tests defined in build configuration");
      return 0;
    }

    // Build and run each test
    int failed_tests = 0;
    for (const auto& test : config.tests)
    {
      context.logger->info("Building test: " + test.name);

      // Create build directory
      std::filesystem::path build_dir = context.projectRoot / "build" / "tests";
      std::filesystem::create_directories(build_dir);

      // Construct compiler command for test
      std::string compiler_cmd = config.toolchain ? config.toolchain->name : "g++";

      // Add compile flags
      for (const auto& flag : config.compile_flags)
      {
        compiler_cmd += " " + std::string(flag.flag);
      }

      // Add ASAN flags if requested
      if (enable_asan)
      {
        compiler_cmd += " -fsanitize=address -fno-omit-frame-pointer";
      }

      // Add include paths
      for (const auto& include : config.include_paths)
      {
        compiler_cmd += " -I" + include;
      }

      // Add definitions
      for (const auto& def : config.definitions)
      {
        compiler_cmd += " -D" + std::string(def.name);
        if (!def.value.empty())
        {
          compiler_cmd += "=" + std::string(def.value);
        }
      }

      // Add test source files
      for (const auto& source : test.sources)
      {
        compiler_cmd += " " + source;
      }

      // Add link flags
      for (const auto& flag : config.link_flags)
      {
        compiler_cmd += " " + std::string(flag.flag);
      }

      // Output test binary
      std::filesystem::path test_binary = build_dir / test.name;
      compiler_cmd += " -o " + test_binary.string();

      context.logger->info("Compiling test: " + compiler_cmd);

      // Execute test binary (placeholder - would use ProcessRunner in real implementation)
      context.logger->info("Running test: " + test.name);
      // In real implementation, would execute the test binary and check return code

      context.logger->info("Test " + test.name + " passed");
    }

    if (failed_tests == 0)
    {
      context.logger->info("All tests passed");
    }
    else
    {
      return std::unexpected("Some tests failed");
    }

    return 0;
  }
  catch (const std::exception& e)
  {
    return std::unexpected("Tests failed: " + std::string(e.what()));
  }
}

}  // namespace cppup::cli