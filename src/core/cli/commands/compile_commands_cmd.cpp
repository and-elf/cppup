#include <filesystem>
#include <string>

#include "../../configuration/compile_commands.hpp"
#include "common.h"

namespace cppup::cli
{

namespace conf = cppup::configuration;

std::expected<int, std::string> executeCompileCommands(conf::BuildOptions    options,
                                                       const CommandContext& context) noexcept
{
  try
  {
    auto& logger = *context.logger;
    logger.info("emitting compile_commands.json...");

    const auto build_file = context.projectRoot / "build.cpp";
    if (!std::filesystem::exists(build_file))
    {
      return std::unexpected("No build.cpp found in: " + context.projectRoot.string());
    }

    const auto cppup_dir = context.projectRoot / ".cppup";
    const auto build_dir = context.projectRoot / "build";

    conf::CompilerOptions compiler_opts;
    compiler_opts.include_paths.push_back((context.projectRoot / "include").string());
    compiler_opts.include_paths.push_back((context.projectRoot / "src").string());
    compiler_opts.output_directory = (cppup_dir / "build" / "config").string();

    conf::ConfigurationCompiler compiler(std::move(compiler_opts));
    auto                        compile_result = compiler.compile(build_file);
    if (!compile_result.success)
    {
      return std::unexpected("compile build.cpp failed: " + compile_result.error_message);
    }

    auto config_result = conf::load_from_library(compile_result.shared_library_path);
    if (!config_result)
    {
      return std::unexpected("load build configuration failed: " + config_result.error());
    }

    auto cc = conf::emit_compile_commands(*config_result, context.projectRoot, build_dir, options);
    if (!cc)
    {
      return std::unexpected("compile_commands.json: " + cc.error());
    }

    logger.info("wrote " + cc->string());
    return 0;
  }
  catch (const std::exception& e)
  {
    return std::unexpected(std::string{"compile-commands failed: "} + e.what());
  }
}

}  // namespace cppup::cli
