#include "common.h"

#include "../../configuration/compile_commands.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace cppup::cli
{

namespace
{

namespace conf = cppup::configuration;
namespace bld  = cppup::build;

void append_common_flags(std::vector<std::string>&     out,
                         const conf::BuildConfiguration& config,
                         const std::filesystem::path&  project_root,
                         bool                          enable_asan)
{
  for (const auto& flag : config.compile_flags) out.emplace_back(flag.flag);
  for (const auto& def : config.definitions)
  {
    std::string d = "-D" + std::string(def.name);
    if (!def.value.empty()) d += "=" + std::string(def.value);
    out.push_back(std::move(d));
  }
  for (const auto& inc : config.include_paths)
  {
    out.push_back("-I" + (project_root / inc).string());
  }
  if (enable_asan)
  {
    out.emplace_back("-fsanitize=address");
    out.emplace_back("-fno-omit-frame-pointer");
  }
}

std::vector<bld::FileDependency> collect_dependencies(
    bld::BuildCache* cache, const std::vector<std::filesystem::path>& sources,
    const std::vector<std::filesystem::path>& include_paths)
{
  std::vector<bld::FileDependency> deps;
  if (!cache) return deps;

  for (const auto& source : sources)
  {
    if (!std::filesystem::exists(source)) continue;
    bld::FileDependency dep;
    dep.file_path     = source;
    dep.last_modified = std::filesystem::last_write_time(source);
    auto checksum     = cache->calculate_file_checksum(source);
    if (checksum) dep.checksum = *checksum;

    auto includes = bld::DependencyScanner::scan_includes(source);
    if (includes)
    {
      for (const auto& inc : *includes)
      {
        for (const auto& dir : include_paths)
        {
          auto resolved = dir / inc;
          if (std::filesystem::exists(resolved))
          {
            dep.includes.push_back(resolved);
            break;
          }
        }
      }
    }
    deps.push_back(std::move(dep));
  }
  return deps;
}

std::expected<std::filesystem::path, std::string> compile_object(
    const std::string& compiler, const std::filesystem::path& source,
    const std::filesystem::path&    obj_path,
    const std::vector<std::string>& flags, Logger& logger)
{
  std::ostringstream cmd;
  cmd << compiler << " -c";
  for (const auto& f : flags) cmd << ' ' << f;
  cmd << ' ' << source.string() << " -o " << obj_path.string();

  logger.debug("compile: " + cmd.str());
  if (std::system(cmd.str().c_str()) != 0)
  {
    return std::unexpected("compilation failed: " + source.string());
  }
  return obj_path;
}

std::expected<std::filesystem::path, std::string> build_library(
    const conf::BuildConfiguration& config, const conf::Library& library,
    const std::filesystem::path& project_root, const std::filesystem::path& build_dir,
    bld::BuildCache* cache, bool enable_asan, Logger& logger, std::size_t& cached_counter)
{
  bld::BuildTarget target;
  target.name = library.name;
  target.type = "library";
  const char* ext    = (library.type == conf::LibraryType::Static) ? ".a" : ".so";
  target.output_path = build_dir / ("lib" + library.name + ext);
  for (const auto& src : library.sources) target.source_files.push_back(project_root / src);

  std::vector<std::string> compile_flags;
  append_common_flags(compile_flags, config, project_root, enable_asan);
  target.compile_flags = compile_flags;
  for (const auto& inc : config.include_paths)
    target.include_paths.push_back(project_root / inc);

  if (cache)
  {
    auto need = cache->needs_rebuild(target);
    if (need && !*need)
    {
      logger.info("cached library: " + library.name);
      ++cached_counter;
      return target.output_path;
    }
  }

  logger.info("building library: " + library.name);
  std::vector<std::filesystem::path> objects;
  objects.reserve(target.source_files.size());
  for (const auto& src : target.source_files)
  {
    auto obj = build_dir / (src.stem().string() + "_" + library.name + ".o");
    auto rc  = compile_object("g++", src, obj, compile_flags, logger);
    if (!rc) return std::unexpected(rc.error());
    objects.push_back(obj);
  }

  std::ostringstream ar_cmd;
  ar_cmd << "ar rcs " << target.output_path.string();
  for (const auto& obj : objects) ar_cmd << ' ' << obj.string();
  if (std::system(ar_cmd.str().c_str()) != 0)
  {
    return std::unexpected("archive failed: " + library.name);
  }

  if (cache)
  {
    auto deps = collect_dependencies(cache, target.source_files, target.include_paths);
    (void) cache->cache_build_result(target, deps);
  }
  return target.output_path;
}

std::expected<void, std::string> build_executable(
    const std::string& kind, const std::string& name,
    const std::vector<std::string>& sources, const conf::BuildConfiguration& config,
    const std::vector<conf::Library>& libraries, const std::filesystem::path& project_root,
    const std::filesystem::path& build_dir, bld::BuildCache* cache, bool enable_asan,
    Logger& logger, std::size_t& cached_counter)
{
  bld::BuildTarget target;
  target.name        = name;
  target.type        = kind;
  target.output_path = build_dir / name;
  for (const auto& src : sources) target.source_files.push_back(project_root / src);

  std::vector<std::string> compile_flags;
  append_common_flags(compile_flags, config, project_root, enable_asan);
  target.compile_flags = compile_flags;
  for (const auto& inc : config.include_paths)
    target.include_paths.push_back(project_root / inc);

  std::vector<std::string> link_flags;
  link_flags.push_back("-L" + build_dir.string());
  if (!libraries.empty()) link_flags.emplace_back("-Wl,--start-group");
  for (const auto& lib : libraries) link_flags.push_back("-l" + lib.name);
  if (!libraries.empty()) link_flags.emplace_back("-Wl,--end-group");
  for (const auto& f : config.link_flags) link_flags.emplace_back(f.flag);
  if (enable_asan) link_flags.emplace_back("-fsanitize=address");
  target.link_flags = link_flags;

  if (cache)
  {
    auto need = cache->needs_rebuild(target);
    if (need && !*need)
    {
      logger.info("cached " + kind + ": " + name);
      ++cached_counter;
      return {};
    }
  }

  logger.info("building " + kind + ": " + name);
  const std::string compiler = config.toolchain ? std::string(config.toolchain->name) : "g++";

  std::ostringstream cmd;
  cmd << compiler;
  for (const auto& f : compile_flags) cmd << ' ' << f;
  for (const auto& src : target.source_files) cmd << ' ' << src.string();
  for (const auto& f : link_flags) cmd << ' ' << f;
  cmd << " -o " << target.output_path.string();

  logger.debug("link: " + cmd.str());
  if (std::system(cmd.str().c_str()) != 0)
  {
    return std::unexpected(kind + " link failed: " + name);
  }

  if (cache)
  {
    auto deps = collect_dependencies(cache, target.source_files, target.include_paths);
    (void) cache->cache_build_result(target, deps);
  }
  return {};
}

}  // namespace

std::expected<int, std::string> executeBuild(bool                  enable_asan,
                                             const CommandContext& context) noexcept
{
  try
  {
    auto& logger = *context.logger;
    logger.info("building project...");

    const auto build_file = context.projectRoot / "build.cpp";
    if (!std::filesystem::exists(build_file))
    {
      return std::unexpected("No build.cpp found in: " + context.projectRoot.string());
    }

    const auto cppup_dir = context.projectRoot / ".cppup";
    const auto cache_dir = cppup_dir / "cache";
    const auto build_dir = context.projectRoot / "build";
    std::filesystem::create_directories(build_dir);

    std::unique_ptr<bld::BuildCache> cache;
    if (auto c = bld::create_build_cache(cache_dir, nullptr)) cache = std::move(*c);
    else
      logger.warning("build cache unavailable: " + c.error());

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

    const auto& config = *config_result;
    logger.info("build configuration loaded");

    if (auto cc = conf::emit_compile_commands(config, context.projectRoot, build_dir,
                                              enable_asan))
    {
      logger.debug("wrote " + cc->string());
    }
    else
    {
      logger.warning("compile_commands.json: " + cc.error());
    }

    std::size_t built  = 0;
    std::size_t cached = 0;

    for (const auto& library : config.libraries)
    {
      auto r = build_library(config, library, context.projectRoot, build_dir, cache.get(),
                             enable_asan, logger, cached);
      if (!r) return std::unexpected(r.error());
      ++built;
    }

    for (const auto& binary : config.binaries)
    {
      auto r =
          build_executable("binary", binary.name, binary.sources, config, config.libraries,
                           context.projectRoot, build_dir, cache.get(), enable_asan, logger, cached);
      if (!r) return std::unexpected(r.error());
      ++built;
    }

    for (const auto& test : config.tests)
    {
      auto r =
          build_executable("test", test.name, test.sources, config, config.libraries,
                           context.projectRoot, build_dir, cache.get(), enable_asan, logger, cached);
      if (!r) return std::unexpected(r.error());
      ++built;
    }

    if (!config.build_steps.empty())
    {
      conf::BuildStepExecutor executor;
      auto                    step_result = executor.execute_build_steps(config);
      if (!step_result.success)
      {
        return std::unexpected("build step failed: " + step_result.error_message);
      }
    }

    logger.info("build complete: " + std::to_string(built) + " built, " +
                std::to_string(cached) + " cached");
    if (cache)
    {
      auto stats = cache->get_stats();
      if (stats)
        logger.info("cache hit rate: " +
                    std::to_string(static_cast<int>(stats->hit_rate * 100.0)) + "%");
    }
    return 0;
  }
  catch (const std::exception& e)
  {
    return std::unexpected(std::string{"build failed: "} + e.what());
  }
}

}  // namespace cppup::cli
