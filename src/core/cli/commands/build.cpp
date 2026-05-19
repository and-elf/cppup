#include <atomic>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "../../configuration/compile_commands.hpp"
#include "../../configuration/link_resolution.hpp"
#include "../../configuration/subproject_loader.hpp"
#include "common.h"

namespace cppup::cli
{

namespace
{

namespace conf = cppup::configuration;
namespace bld  = cppup::build;

void append_common_flags(std::vector<std::string>& out, const conf::BuildConfiguration& config,
                         const std::filesystem::path& project_root, conf::BuildOptions options)
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
  if (conf::enabled(options.asan))
  {
    out.emplace_back("-fsanitize=address");
    out.emplace_back("-fno-omit-frame-pointer");
  }
  if (conf::enabled(options.coverage))
  {
    out.emplace_back("--coverage");
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

struct CompileTask
{
  std::filesystem::path source;
  std::filesystem::path object;
};

// Run `work` over each item in `items` across up to `jobs` worker threads.
// `jobs == 0` means auto. Returns the first error encountered; in-flight work
// completes naturally (we don't try to cancel). Used to parallelize the outer
// per-target build loops (tests, binaries).
template <typename Item, typename Work>
std::expected<void, std::string> run_in_parallel(const std::vector<Item>& items, unsigned jobs,
                                                 Work work)
{
  if (items.empty()) return {};

  unsigned worker_count =
      jobs != 0U ? jobs : std::max(1U, std::thread::hardware_concurrency());
  worker_count = static_cast<unsigned>(std::min<std::size_t>(worker_count, items.size()));

  std::mutex               err_mu;
  std::atomic<std::size_t> next{0};
  std::atomic<bool>        aborted{false};
  std::string              first_err;

  auto worker = [&]()
  {
    while (!aborted.load(std::memory_order_relaxed))
    {
      const std::size_t i = next.fetch_add(1, std::memory_order_relaxed);
      if (i >= items.size()) return;
      auto r = work(items[i]);
      if (!r)
      {
        const std::lock_guard<std::mutex> lk(err_mu);
        if (!aborted.exchange(true))
        {
          first_err = r.error();
        }
        return;
      }
    }
  };

  std::vector<std::thread> threads;
  threads.reserve(worker_count);
  for (unsigned i = 0; i < worker_count; ++i)
  {
    threads.emplace_back(worker);
  }
  for (auto& th : threads) th.join();

  if (aborted.load()) return std::unexpected(first_err);
  return {};
}

// Compile a batch of sources concurrently. `jobs == 0` means auto (hardware
// concurrency). On the first failure, the abort flag short-circuits remaining
// work and the first error is returned; in-flight compilations finish naturally
// (we don't try to kill child processes). The logger is serialized so debug
// lines don't interleave.
std::expected<void, std::string> compile_objects_parallel(
    const std::string& compiler, const std::vector<CompileTask>& tasks,
    const std::vector<std::string>& flags, unsigned jobs, Logger& logger)
{
  if (tasks.empty()) return {};

  unsigned worker_count =
      jobs != 0U ? jobs : std::max(1U, std::thread::hardware_concurrency());
  worker_count = static_cast<unsigned>(std::min<std::size_t>(worker_count, tasks.size()));

  std::mutex               log_mu;
  std::mutex               err_mu;
  std::atomic<std::size_t> next{0};
  std::atomic<bool>        aborted{false};
  std::string              first_err;

  auto worker = [&]()
  {
    while (!aborted.load(std::memory_order_relaxed))
    {
      const std::size_t i = next.fetch_add(1, std::memory_order_relaxed);
      if (i >= tasks.size()) return;
      const auto&        t = tasks[i];
      std::ostringstream cmd;
      cmd << compiler << " -c";
      for (const auto& f : flags) cmd << ' ' << f;
      cmd << ' ' << t.source.string() << " -o " << t.object.string();
      {
        const std::lock_guard<std::mutex> lk(log_mu);
        logger.debug("compile: " + cmd.str());
      }
      if (std::system(cmd.str().c_str()) != 0)
      {
        const std::lock_guard<std::mutex> lk(err_mu);
        if (!aborted.exchange(true))
        {
          first_err = "compilation failed: " + t.source.string();
        }
        return;
      }
    }
  };

  std::vector<std::thread> threads;
  threads.reserve(worker_count);
  for (unsigned i = 0; i < worker_count; ++i)
  {
    threads.emplace_back(worker);
  }
  for (auto& th : threads) th.join();

  if (aborted.load()) return std::unexpected(first_err);
  return {};
}

std::expected<std::filesystem::path, std::string> build_library(
    const conf::BuildConfiguration& config, const conf::Library& library,
    const std::filesystem::path& project_root, const std::filesystem::path& build_dir,
    bld::BuildCache* cache, conf::BuildOptions options, Logger& logger,
    std::size_t& cached_counter)
{
  bld::BuildTarget target;
  target.name        = library.name;
  target.type        = "library";
  const char* ext    = (library.type == conf::LibraryType::Static) ? ".a" : ".so";
  target.output_path = build_dir / ("lib" + library.name + ext);
  for (const auto& src : library.sources) target.source_files.push_back(project_root / src);

  std::vector<std::string> compile_flags;
  append_common_flags(compile_flags, config, project_root, options);
  target.compile_flags = compile_flags;
  for (const auto& inc : config.include_paths) target.include_paths.push_back(project_root / inc);

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
  std::vector<CompileTask>           lib_tasks;
  std::vector<std::filesystem::path> objects;
  lib_tasks.reserve(target.source_files.size());
  objects.reserve(target.source_files.size());
  for (const auto& src : target.source_files)
  {
    auto obj = build_dir / (src.stem().string() + "_" + library.name + ".o");
    lib_tasks.push_back({src, obj});
    objects.push_back(obj);
  }
  if (auto rc = compile_objects_parallel("g++", lib_tasks, compile_flags, options.jobs, logger);
      !rc)
  {
    return std::unexpected(rc.error());
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
    const std::string& kind, const std::string& name, const std::vector<std::string>& sources,
    const conf::BuildConfiguration& config, const std::vector<std::string>& linked_library_names,
    const std::filesystem::path& project_root, const std::filesystem::path& build_dir,
    bld::BuildCache* cache, conf::BuildOptions options, Logger& logger,
    std::size_t& cached_counter, const std::vector<std::string>& extra_link_flags = {},
    const std::filesystem::path& output_dir = {})
{
  const auto& target_dir = output_dir.empty() ? build_dir : output_dir;
  auto resolved = conf::resolve_link_set(linked_library_names, config.libraries);
  if (!resolved)
  {
    return std::unexpected(kind + " " + name + ": " + resolved.error());
  }
  const auto library_link_flags = conf::aggregate_link_flags(*resolved, config.libraries);

  bld::BuildTarget target;
  target.name        = name;
  target.type        = kind;
  target.output_path = target_dir / name;
  for (const auto& src : sources)
  {
    target.source_files.push_back(project_root / src);
  }

  std::vector<std::string> compile_flags;
  append_common_flags(compile_flags, config, project_root, options);
  target.compile_flags = compile_flags;
  for (const auto& inc : config.include_paths)
  {
    target.include_paths.push_back(project_root / inc);
  }

  std::vector<std::string> link_flags;
  link_flags.push_back("-L" + build_dir.string());
  if (!resolved->empty())
  {
    link_flags.emplace_back("-Wl,--start-group");
    for (const auto& lib : *resolved)
    {
      link_flags.push_back("-l" + lib);
    }
    link_flags.emplace_back("-Wl,--end-group");
  }
  for (const auto& f : library_link_flags)
  {
    link_flags.emplace_back(f);
  }
  for (const auto& f : config.link_flags)
  {
    link_flags.emplace_back(f.flag);
  }
  for (const auto& f : extra_link_flags)
  {
    link_flags.emplace_back(f);
  }
  if (conf::enabled(options.asan))
  {
    link_flags.emplace_back("-fsanitize=address");
  }
  if (conf::enabled(options.coverage))
  {
    link_flags.emplace_back("--coverage");
  }
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

  // Split compile and link so per-source compilation can run in parallel under
  // -j. Object filenames include kind+name to avoid colliding with library
  // objects or other binaries that share a source basename.
  std::vector<CompileTask>           bin_tasks;
  std::vector<std::filesystem::path> bin_objects;
  bin_tasks.reserve(target.source_files.size());
  bin_objects.reserve(target.source_files.size());
  for (const auto& src : target.source_files)
  {
    auto obj = build_dir / (src.stem().string() + "_" + name + "_" + kind + ".o");
    bin_tasks.push_back({src, obj});
    bin_objects.push_back(obj);
  }
  if (auto rc =
          compile_objects_parallel(compiler, bin_tasks, compile_flags, options.jobs, logger);
      !rc)
  {
    return std::unexpected(rc.error());
  }

  std::ostringstream cmd;
  cmd << compiler;
  for (const auto& obj : bin_objects) cmd << ' ' << obj.string();
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

std::expected<int, std::string> executeBuild(conf::BuildOptions    options,
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
    if (auto c = bld::create_build_cache(cache_dir, nullptr))
      cache = std::move(*c);
    else
      logger.warning("build cache unavailable: " + c.error());

    conf::CompilerOptions compiler_opts;
    compiler_opts.include_paths.push_back((context.projectRoot / "include").string());
    compiler_opts.include_paths.push_back((context.projectRoot / "src").string());
    compiler_opts.output_directory = (cppup_dir / "build" / "config").string();

    conf::ConfigurationCompiler compiler(std::move(compiler_opts));
    auto config_result = conf::load_with_subprojects(context.projectRoot, compiler);
    if (!config_result)
    {
      return std::unexpected("load build configuration failed: " + config_result.error());
    }

    const auto& config = *config_result;
    logger.info("build configuration loaded (" + std::to_string(config.libraries.size()) +
                " libraries, " + std::to_string(config.binaries.size()) + " binaries)");

    if (auto cc = conf::emit_compile_commands(config, context.projectRoot, build_dir, options))
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
      auto r = build_library(config, library, context.projectRoot, build_dir, cache.get(), options,
                             logger, cached);
      if (!r) return std::unexpected(r.error());
      ++built;
    }

    // Tests/binaries are independent and typically one source each — outer
    // parallelism across targets is a bigger win than inner per-source. Force
    // inner jobs=1 in the workers so we don't run jobs² g++ processes.
    conf::BuildOptions inner_opts = options;
    inner_opts.jobs               = 1;

    std::atomic<std::size_t> bin_cached{0};
    if (auto r = run_in_parallel(
            config.binaries, options.jobs,
            [&](const conf::Binary& binary) -> std::expected<void, std::string>
            {
              std::size_t local_cached = 0;
              auto        rr =
                  build_executable("binary", binary.name, binary.sources, config, binary.libraries,
                                   context.projectRoot, build_dir, cache.get(), inner_opts, logger,
                                   local_cached);
              if (!rr) return std::unexpected(rr.error());
              bin_cached.fetch_add(local_cached, std::memory_order_relaxed);
              return {};
            });
        !r)
    {
      return std::unexpected(r.error());
    }
    built += config.binaries.size();
    cached += bin_cached.load();

    // Tests link only the internal libraries they explicitly name in
    // Test::libraries, plus any verbatim flags in Test::link_flags
    // (e.g. "-lgtest -lgtest_main -lpthread"). Binaries land in build/tests/
    // so the test runner and VSCode testMate glob can both find them.
    const auto tests_dir = build_dir / "tests";
    if (!config.tests.empty())
    {
      std::filesystem::create_directories(tests_dir);
    }

    std::atomic<std::size_t> test_cached{0};
    if (auto r =
            run_in_parallel(config.tests, options.jobs,
                            [&](const conf::Test& test) -> std::expected<void, std::string>
                            {
                              std::vector<std::string> test_link_flag_strings;
                              test_link_flag_strings.reserve(test.link_flags.size());
                              for (const auto& f : test.link_flags)
                              {
                                test_link_flag_strings.emplace_back(f.flag);
                              }
                              std::size_t local_cached = 0;
                              auto        rr           = build_executable(
                                  "test", test.name, test.sources, config, test.libraries,
                                  context.projectRoot, build_dir, cache.get(), inner_opts, logger,
                                  local_cached, test_link_flag_strings, tests_dir);
                              if (!rr) return std::unexpected(rr.error());
                              test_cached.fetch_add(local_cached, std::memory_order_relaxed);
                              return {};
                            });
        !r)
    {
      return std::unexpected(r.error());
    }
    built += config.tests.size();
    cached += test_cached.load();

    if (!config.build_steps.empty())
    {
      conf::BuildStepExecutor executor;
      auto                    step_result = executor.execute_build_steps(config);
      if (!step_result.success)
      {
        return std::unexpected("build step failed: " + step_result.error_message);
      }
    }

    logger.info("build complete: " + std::to_string(built) + " built, " + std::to_string(cached) +
                " cached");
    if (cache)
    {
      auto stats = cache->get_stats();
      if (stats)
        logger.info("cache hit rate: " + std::to_string(static_cast<int>(stats->hit_rate * 100.0)) +
                    "%");
    }
    return 0;
  }
  catch (const std::exception& e)
  {
    return std::unexpected(std::string{"build failed: "} + e.what());
  }
}

}  // namespace cppup::cli
