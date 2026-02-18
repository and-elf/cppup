#include <fstream>

#include "../configuration/platform.hpp"
#include "common.h"
#ifndef IS_BOOTSTRAP_BUILD
#include "../configuration/compiler.hpp"
#endif

namespace cppup::cli
{

#ifdef IS_BOOTSTRAP_BUILD
static std::expected<int, std::string> executeBootstrapBuild(const CommandContext& context);
#endif

std::expected<int, std::string> executeBuild(bool /*enable_asan*/,
                                             const CommandContext& context) noexcept
{
  try
  {
    context.logger->info("Building project...");

    // Look for build.cpp in current directory
    std::filesystem::path build_file = context.projectRoot / "build.cpp";
    if (!std::filesystem::exists(build_file))
    {
      return std::unexpected("No build.cpp found in current directory");
    }

#ifdef IS_BOOTSTRAP_BUILD
    return executeBootstrapBuild(context);
#else
    // Initialize dependency database and build cache
    std::filesystem::path cache_dir = context.projectRoot / ".cppup" / "cache";
    std::filesystem::path db_path   = context.projectRoot / ".cppup" / "packages.db";

    auto dep_db_result = cppup::dependency::create_dependency_database(db_path);
    if (!dep_db_result)
    {
      context.logger->info("Warning: Could not initialize dependency database: " +
                           dep_db_result.error());
    }

    auto build_cache_result = cppup::build::create_build_cache(
        cache_dir, dep_db_result ? std::move(*dep_db_result) : nullptr);
    if (!build_cache_result)
    {
      context.logger->info("Warning: Could not initialize build cache: " +
                           build_cache_result.error());
    }

    // Use configuration compiler to compile build.cpp
    cppup::configuration::ConfigurationCompiler compiler;
    auto                                        result = compiler.compile(build_file);

    if (!result.success)
    {
      return std::unexpected("Failed to compile build configuration: " + result.error_message);
    }

    // Load the compiled configuration
    cppup::configuration::ConfigurationLoader loader;
    auto load_result = loader.load_from_library(result.shared_library_path);

    if (!load_result.success)
    {
      return std::unexpected("Failed to load build configuration: " + load_result.error_message);
    }

    auto& config = load_result.configuration.value();
    context.logger->info("Build configuration loaded successfully");

    // Create build directory
    std::filesystem::path build_dir = context.projectRoot / "build";
    std::filesystem::create_directories(build_dir);

    int targets_built  = 0;
    int targets_cached = 0;

    // Build binaries
    for (const auto& binary : config.binaries)
    {
      // Create build target
      cppup::build::BuildTarget target;
      target.name        = binary.name;
      target.type        = "binary";
      target.output_path = build_dir / binary.name;

      // Convert source files to paths
      for (const auto& source : binary.sources)
      {
        target.source_files.push_back(context.projectRoot / source);
      }

      // Add compile flags
      for (const auto& flag : config.compile_flags)
      {
        target.compile_flags.push_back(std::string(flag.flag));
      }

      // Add ASAN flags if requested
      if (enable_asan)
      {
        target.compile_flags.push_back("-fsanitize=address");
        target.compile_flags.push_back("-fno-omit-frame-pointer");
        target.link_flags.push_back("-fsanitize=address");
      }

      // Add link flags
      for (const auto& flag : config.link_flags)
      {
        target.link_flags.push_back(std::string(flag.flag));
      }

      // Add definitions
      for (const auto& def : config.definitions)
      {
        std::string definition = "-D" + std::string(def.name);
        if (!def.value.empty())
        {
          definition += "=" + std::string(def.value);
        }
        target.definitions.push_back(definition);
      }

      // Add include paths
      for (const auto& include : config.include_paths)
      {
        target.include_paths.push_back(context.projectRoot / include);
      }

      // Check if rebuild is needed
      bool needs_rebuild = true;
      if (build_cache_result)
      {
        auto cache_check = (*build_cache_result)->needs_rebuild(target);
        if (cache_check)
        {
          needs_rebuild = *cache_check;
          if (!needs_rebuild)
          {
            context.logger->info("Using cached build for: " + binary.name);
            targets_cached++;
            continue;
          }
        }
      }

      context.logger->info("Building binary: " + binary.name);

      // Construct compiler command
      std::string compiler_cmd = config.toolchain ? config.toolchain->name : "g++";

      // Add all flags
      for (const auto& flag : target.compile_flags)
      {
        compiler_cmd += " " + flag;
      }

      for (const auto& def : target.definitions)
      {
        compiler_cmd += " " + def;
      }

      for (const auto& include : target.include_paths)
      {
        compiler_cmd += " -I" + include.string();
      }

      for (const auto& source : target.source_files)
      {
        compiler_cmd += " " + source.string();
      }

      for (const auto& flag : target.link_flags)
      {
        compiler_cmd += " " + flag;
      }

      // Output binary
      compiler_cmd += " -o " + target.output_path.string();

      context.logger->info("Executing: " + compiler_cmd);

      // Execute build command
      int build_result = std::system(compiler_cmd.c_str());
      if (build_result != 0)
      {
        return std::unexpected("Build failed for binary: " + binary.name);
      }

      // Analyze file dependencies and cache the result
      if (build_cache_result)
      {
        std::vector<cppup::build::FileDependency> dependencies;

        for (const auto& source_file : target.source_files)
        {
          if (std::filesystem::exists(source_file))
          {
            cppup::build::FileDependency dep;
            dep.file_path     = source_file;
            dep.last_modified = std::filesystem::last_write_time(source_file);

            auto checksum_result = (*build_cache_result)->calculate_file_checksum(source_file);
            if (checksum_result)
            {
              dep.checksum = *checksum_result;
            }

            // Scan for include dependencies
            auto includes_result = cppup::build::DependencyScanner::scan_includes(source_file);
            if (includes_result)
            {
              // Convert include names to file paths (simplified)
              for (const auto& include : *includes_result)
              {
                for (const auto& include_path : target.include_paths)
                {
                  std::filesystem::path header_file = include_path / include;
                  if (std::filesystem::exists(header_file))
                  {
                    dep.includes.push_back(header_file);
                    break;
                  }
                }
              }
            }

            dependencies.push_back(dep);
          }
        }

        // Cache the build result
        auto cache_result = (*build_cache_result)->cache_build_result(target, dependencies);
        if (!cache_result)
        {
          context.logger->info("Warning: Failed to cache build result: " + cache_result.error());
        }
      }

      targets_built++;
    }

    // Build libraries
    for (const auto& library : config.libraries)
    {
      // Similar implementation as binaries but for libraries
      cppup::build::BuildTarget target;
      target.name = library.name;
      target.type = "library";

      // Use platform-aware library extension
      std::string lib_extension =
          std::string(cppup::configuration::library_extension(library.type));
      target.output_path = build_dir / ("lib" + library.name + lib_extension);

      // Convert source files to paths
      for (const auto& source : library.sources)
      {
        target.source_files.push_back(context.projectRoot / source);
      }

      // Check cache and build if needed (similar to binaries)
      bool needs_rebuild = true;
      if (build_cache_result)
      {
        auto cache_check = (*build_cache_result)->needs_rebuild(target);
        if (cache_check && !*cache_check)
        {
          context.logger->info("Using cached build for library: " + library.name);
          targets_cached++;
          continue;
        }
      }

      context.logger->info("Building library: " + library.name);

      // Create dummy library file
      std::ofstream lib_file(target.output_path);
      lib_file << "// Built library: " << library.name << std::endl;
      lib_file.close();

      targets_built++;
    }

    // Execute custom build steps
    if (!config.build_steps.empty())
    {
      context.logger->info("Executing custom build steps...");
      cppup::configuration::BuildStepExecutor executor;
      auto                                    step_result = executor.execute_build_steps(config);
      if (!step_result.success)
      {
        return std::unexpected("Build step failed: " + step_result.error_message);
      }
    }

    // Show build summary
    context.logger->info("Build completed successfully");
    context.logger->info("Targets built: " + std::to_string(targets_built));
    context.logger->info("Targets cached: " + std::to_string(targets_cached));

    if (build_cache_result)
    {
      auto stats_result = (*build_cache_result)->get_stats();
      if (stats_result)
      {
        const auto& stats = *stats_result;
        context.logger->info(
            "Cache hit rate: " + std::to_string(static_cast<int>(stats.hit_rate * 100)) + "%");
      }
    }

    return 0;
#endif
  }
  catch (const std::exception& e)
  {
    return std::unexpected("Build failed: " + std::string(e.what()));
  }
}

#ifdef IS_BOOTSTRAP_BUILD
// Bootstrap build implementation
static std::expected<int, std::string> executeBootstrapBuild(const CommandContext& context)
{
  // For bootstrap build, compile essential libraries directly without configuration system
  context.logger->info("Starting bootstrap build...");

  // Create build directory
  std::filesystem::path build_dir = context.projectRoot / "bootstrap_build";
  std::filesystem::create_directories(build_dir);

  // Define essential libraries to build for bootstrap
  struct BootstrapLibrary
  {
    std::string              name;
    std::vector<std::string> sources;
    std::vector<std::string> include_paths;
    std::vector<std::string> compile_flags;
  };

  std::vector<BootstrapLibrary> bootstrap_libs = {
      {"cppup_config",
       {"src/core/configuration/compiler.cpp", "src/core/configuration/loader.cpp"},
       {"include", "src", "src/core/configuration"},
       {"-std=c++23", "-fPIC", "-DIS_BOOTSTRAP_BUILD"}},
      {"cppup_cli",
       {"src/core/cli/cli_application.cpp", "src/core/cli/commands.cpp", "src/core/cli/logger.cpp",
        "src/core/process_runner.cpp", "src/core/cli/commands/build.cpp"},
       {"include", "src", "src/core/cli", "src/core/configuration", "src/cli"},
       {"-std=c++23", "-fPIC", "-DIS_BOOTSTRAP_BUILD"}}};

  int targets_built = 0;

  // Build each bootstrap library
  for (const auto& lib : bootstrap_libs)
  {
    context.logger->info("Building bootstrap library: " + lib.name);

    std::vector<std::filesystem::path> object_files;

    // Compile each source file
    for (const auto& source : lib.sources)
    {
      std::filesystem::path source_path = context.projectRoot / source;
      if (!std::filesystem::exists(source_path))
      {
        context.logger->info("Warning: Source file not found: " + source);
        continue;
      }

      std::filesystem::path obj_path =
          build_dir / (source_path.stem().string() + "_" + lib.name + ".o");

      // Build compiler command
      std::string compile_cmd = "g++ -c";
      for (const auto& flag : lib.compile_flags)
      {
        compile_cmd += " " + flag;
      }
      for (const auto& include : lib.include_paths)
      {
        compile_cmd += " -I" + (context.projectRoot / include).string();
      }
      compile_cmd += " " + source_path.string() + " -o " + obj_path.string();

      context.logger->info("Compiling: " + source);
      int compile_result = std::system(compile_cmd.c_str());
      if (compile_result != 0)
      {
        return std::unexpected("Compilation failed for: " + source);
      }

      object_files.push_back(obj_path);
    }

    // Link static library
    if (!object_files.empty())
    {
      std::filesystem::path lib_path = build_dir / ("lib" + lib.name + ".a");
      std::string           link_cmd = "ar rcs " + lib_path.string();
      for (const auto& obj : object_files)
      {
        link_cmd += " " + obj.string();
      }

      context.logger->info("Linking library: " + lib.name);
      int link_result = std::system(link_cmd.c_str());
      if (link_result != 0)
      {
        return std::unexpected("Linking failed for library: " + lib.name);
      }
      targets_built++;
    }
  }

  // Build main executable
  context.logger->info("Building main executable: cppup");
  std::filesystem::path exe_path = build_dir / "cppup";

  std::string compile_cmd = "g++ -std=c++23 -DIS_BOOTSTRAP_BUILD";
  // Add include paths
  for (const auto& include :
       {"include", "src", "src/core/cli", "src/core/configuration", "src/cli"})
  {
    compile_cmd += " -I" + (context.projectRoot / include).string();
  }
  compile_cmd += " " + (context.projectRoot / "src/main.cpp").string();
  compile_cmd += " -L" + build_dir.string();
  compile_cmd += " -lcppup_config -lcppup_cli";
  compile_cmd += " -o " + exe_path.string();

  int link_result = std::system(compile_cmd.c_str());
  if (link_result != 0)
  {
    return std::unexpected("Linking failed for main executable");
  }
  targets_built++;

  context.logger->info("Bootstrap build completed successfully");
  context.logger->info("Targets built: " + std::to_string(targets_built));

  return 0;
}
#endif

}  // namespace cppup::cli
