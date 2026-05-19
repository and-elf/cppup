#include <atomic>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <mutex>
#include <print>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "../../configuration/build_configuration.hpp"
#include "../../configuration/compile_commands.hpp"
#include "../../configuration/link_resolution.hpp"
#include "../../configuration/subproject_loader.hpp"
#include "../../configuration/toolchain_flags.hpp"
#include "build_options.hpp"
#include "common.h"
#include "logger.hpp"

namespace cppup::cli
{

namespace
{

namespace conf = cppup::configuration;
namespace bld  = cppup::build;

void append_common_flags(std::vector<std::string>& out, const conf::BuildConfiguration& config,
                         const std::filesystem::path& project_root, conf::BuildOptions options)
{
  if (config.toolchain)
  {
    for (auto& f : conf::dialect_flags(*config.toolchain)) out.emplace_back(std::move(f));
  }
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

// Per-step progress for the user-facing build output. `set_total` is called
// once after the planning pass walks the cache; each compile/archive/link
// then calls `step(action, name)` to emit one `[n/total] action name` line
// (e.g. `[12/45] compiling src/foo.cpp`). The mutex serializes prints across
// worker threads so lines don't interleave.
struct ProgressReporter
{
  std::mutex               print_mu;
  std::atomic<std::size_t> done{0};
  std::size_t              total{0};
  int                      width{1};

  void set_total(std::size_t t) noexcept
  {
    total = t;
    width = static_cast<int>(std::max<std::size_t>(1, std::to_string(t).size()));
  }

  void step(std::string_view action, std::string_view name)
  {
    if (total == 0)
    {
      return;
    }
    const auto                        n = done.fetch_add(1, std::memory_order_relaxed) + 1;
    const std::lock_guard<std::mutex> lk(print_mu);
    std::print("[{:>{}}/{}] {} {}\n", n, width, total, action, name);
  }
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

  unsigned worker_count = jobs != 0U ? jobs : std::max(1U, std::thread::hardware_concurrency());
  worker_count          = static_cast<unsigned>(std::min<std::size_t>(worker_count, items.size()));

  std::mutex               err_mu;
  std::atomic<std::size_t> next{0};
  std::atomic<bool>        aborted{false};
  std::string              first_err;

  auto worker = [&]()
  {
    while (!aborted.load(std::memory_order_relaxed))
    {
      const std::size_t i = next.fetch_add(1, std::memory_order_relaxed);
      if (i >= items.size())
      {
        return;
      }
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

  if (aborted.load())
  {
    return std::unexpected(first_err);
  }
  return {};
}

// Compile a batch of sources concurrently. `jobs == 0` means auto (hardware
// concurrency). On the first failure, the abort flag short-circuits remaining
// work and the first error is returned; in-flight compilations finish naturally
// (we don't try to kill child processes). The logger is serialized so debug
// lines don't interleave.
std::expected<void, std::string> compile_objects_parallel(const std::string&              compiler,
                                                          const std::vector<CompileTask>& tasks,
                                                          const std::vector<std::string>& flags,
                                                          unsigned jobs, Logger& logger,
                                                          ProgressReporter* progress)
{
  if (tasks.empty())
  {
    return {};
  }

  unsigned worker_count = jobs != 0U ? jobs : std::max(1U, std::thread::hardware_concurrency());
  worker_count          = static_cast<unsigned>(std::min<std::size_t>(worker_count, tasks.size()));

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
      if (i >= tasks.size())
      {
        return;
      }
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
      if (progress != nullptr)
      {
        progress->step("compiling", t.source.string());
      }
    }
  };

  std::vector<std::thread> threads;
  threads.reserve(worker_count);
  for (unsigned i = 0; i < worker_count; ++i)
  {
    threads.emplace_back(worker);
  }
  for (auto& th : threads)
  {
    th.join();
  }

  if (aborted.load())
  {
    return std::unexpected(first_err);
  }
  return {};
}

std::expected<std::filesystem::path, std::string> build_library(
    const conf::BuildConfiguration& config, const conf::Library& library,
    const std::filesystem::path& project_root, const std::filesystem::path& build_dir,
    bld::BuildCache* cache, conf::BuildOptions options, Logger& logger, std::size_t& cached_counter,
    ProgressReporter& progress)
{
  bld::BuildTarget target;
  target.name        = library.name;
  target.type        = "library";
  const char* ext    = (library.type == conf::LibraryType::Static) ? ".a" : ".so";
  target.output_path = build_dir / ("lib" + library.name + ext);
  for (const auto& src : library.sources)
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

  if (cache != nullptr)
  {
    auto need = cache->needs_rebuild(target);
    if (need && !*need)
    {
      logger.info("cached library: " + library.name);
      ++cached_counter;
      return target.output_path;
    }
  }

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
  if (auto rc = compile_objects_parallel("g++", lib_tasks, compile_flags, options.jobs, logger,
                                         &progress);
      !rc)
  {
    return std::unexpected(rc.error());
  }

  std::ostringstream ar_cmd;
  ar_cmd << "ar rcs " << target.output_path.string();
  for (const auto& obj : objects) ar_cmd << ' ' << obj.string();
  logger.debug("archive: " + ar_cmd.str());
  if (std::system(ar_cmd.str().c_str()) != 0)
  {
    return std::unexpected("archive failed: " + library.name);
  }
  progress.step("archiving", target.output_path.filename().string());

  if (cache != nullptr)
  {
    auto deps = collect_dependencies(cache, target.source_files, target.include_paths);
    cache->cache_build_result(target, deps);
  }
  return target.output_path;
}

std::expected<void, std::string> build_executable(
    const std::string& kind, const std::string& name, const std::vector<std::string>& sources,
    const conf::BuildConfiguration& config, const std::vector<std::string>& linked_library_names,
    const std::filesystem::path& project_root, const std::filesystem::path& build_dir,
    bld::BuildCache* cache, conf::BuildOptions options, Logger& logger, std::size_t& cached_counter,
    ProgressReporter& progress, const std::vector<std::string>& extra_link_flags = {},
    const std::filesystem::path& output_dir = {})
{
  const auto& target_dir = output_dir.empty() ? build_dir : output_dir;
  auto        resolved   = conf::resolve_link_set(linked_library_names, config.libraries);
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

  if (cache != nullptr)
  {
    auto need = cache->needs_rebuild(target);
    if (need && !*need)
    {
      logger.info("cached " + kind + ": " + name);
      ++cached_counter;
      return {};
    }
  }

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
  if (auto rc = compile_objects_parallel(compiler, bin_tasks, compile_flags, options.jobs, logger,
                                         &progress);
      !rc)
  {
    return std::unexpected(rc.error());
  }

  std::ostringstream cmd;
  cmd << compiler;
  for (const auto& obj : bin_objects)
  {
    cmd << ' ' << obj.string();
  }
  for (const auto& f : link_flags)
  {
    cmd << ' ' << f;
  }
  cmd << " -o " << target.output_path.string();

  logger.debug("link: " + cmd.str());
  if (std::system(cmd.str().c_str()) != 0)
  {
    return std::unexpected(kind + " link failed: " + name);
  }
  progress.step("linking", target.output_path.filename().string());

  if (cache != nullptr)
  {
    auto deps = collect_dependencies(cache, target.source_files, target.include_paths);
    return cache->cache_build_result(target, deps);
  }
  return {};
}

// Build the same BuildTarget that build_library would, used by the planning
// pass to query the cache without doing real work. Kept in sync with the
// construction inside build_library.
bld::BuildTarget plan_library_target(const conf::BuildConfiguration& config,
                                     const conf::Library&            library,
                                     const std::filesystem::path&    project_root,
                                     const std::filesystem::path&    build_dir,
                                     conf::BuildOptions              options)
{
  bld::BuildTarget target;
  target.name        = library.name;
  target.type        = "library";
  const char* ext    = (library.type == conf::LibraryType::Static) ? ".a" : ".so";
  target.output_path = build_dir / ("lib" + library.name + ext);
  for (const auto& src : library.sources)
  {
    target.source_files.push_back(project_root / src);
  }
  append_common_flags(target.compile_flags, config, project_root, options);
  for (const auto& inc : config.include_paths)
  {
    target.include_paths.push_back(project_root / inc);
  }
  return target;
}

// Same idea as plan_library_target for binaries/tests. Mirrors the link-flag
// composition in build_executable; the cache key depends on link_flags so
// planning must match exactly.
bld::BuildTarget plan_executable_target(
    const std::string& kind, const std::string& name, const std::vector<std::string>& sources,
    const conf::BuildConfiguration& config, const std::vector<std::string>& linked_library_names,
    const std::filesystem::path& project_root, const std::filesystem::path& build_dir,
    conf::BuildOptions options, const std::vector<std::string>& extra_link_flags,
    const std::filesystem::path& output_dir)
{
  const auto&      target_dir = output_dir.empty() ? build_dir : output_dir;
  bld::BuildTarget target;
  target.name        = name;
  target.type        = kind;
  target.output_path = target_dir / name;
  for (const auto& src : sources)
  {
    target.source_files.push_back(project_root / src);
  }
  append_common_flags(target.compile_flags, config, project_root, options);
  for (const auto& inc : config.include_paths)
  {
    target.include_paths.push_back(project_root / inc);
  }

  auto resolved = conf::resolve_link_set(linked_library_names, config.libraries);
  if (resolved)
  {
    const auto library_link_flags = conf::aggregate_link_flags(*resolved, config.libraries);
    target.link_flags.push_back("-L" + build_dir.string());
    if (!resolved->empty())
    {
      target.link_flags.emplace_back("-Wl,--start-group");
      for (const auto& lib : *resolved)
      {
        target.link_flags.push_back("-l" + lib);
      }
      target.link_flags.emplace_back("-Wl,--end-group");
    }
    for (const auto& f : library_link_flags)
    {
      target.link_flags.emplace_back(f);
    }
  }
  for (const auto& f : config.link_flags)
  {
    target.link_flags.emplace_back(f.flag);
  }
  for (const auto& f : extra_link_flags)
  {
    target.link_flags.emplace_back(f);
  }
  if (conf::enabled(options.asan))
  {
    target.link_flags.emplace_back("-fsanitize=address");
  }
  if (conf::enabled(options.coverage))
  {
    target.link_flags.emplace_back("--coverage");
  }
  return target;
}

// Hand off a non-Cppup subproject to its native build system. The external
// build's stdout/stderr flows through directly — we don't wrap it in
// progress reporting.
std::expected<void, std::string> run_external_subproject(const conf::Subproject&      sp,
                                                         const std::filesystem::path& sp_dir,
                                                         Logger&                      logger)
{
  if (!sp.build_system)
  {
    return {};
  }
  switch (*sp.build_system)
  {
    case conf::BuildSystem::CMake:
    {
      logger.info("Using CMake for subproject " + sp.path);
      std::ostringstream cfg;
      cfg << "cmake -S " << sp_dir.string() << " -B " << (sp_dir / "build").string();
      for (const auto& a : sp.build_args)
      {
        cfg << ' ' << a;
      }
      if (std::system(cfg.str().c_str()) != 0)
      {
        return std::unexpected("subproject " + sp.path + ": cmake configure failed");
      }
      std::ostringstream bld_cmd;
      bld_cmd << "cmake --build " << (sp_dir / "build").string();
      if (std::system(bld_cmd.str().c_str()) != 0)
      {
        return std::unexpected("subproject " + sp.path + ": cmake build failed");
      }
      return {};
    }
    case conf::BuildSystem::Make:
    {
      logger.info("Using Make for subproject " + sp.path);
      std::ostringstream make_cmd;
      make_cmd << "make -C " << sp_dir.string();
      for (const auto& a : sp.build_args)
      {
        make_cmd << ' ' << a;
      }
      if (std::system(make_cmd.str().c_str()) != 0)
      {
        return std::unexpected("subproject " + sp.path + ": make failed");
      }
      return {};
    }
    case conf::BuildSystem::HeaderOnly:
      logger.info("Using header-only subproject " + sp.path);
      return {};
    case conf::BuildSystem::Cppup:
      // Cppup subprojects were already merged in load_with_subprojects;
      // they should not appear here.
      return {};
  }
  return {};
}

// Walk libraries/binaries/tests, query the cache, and return the number of
// progress steps the build will emit: compile-per-source plus one finalize
// step (archive for libraries, link for binaries/tests). Cached targets are
// skipped — they emit a single "cached X" line instead, outside progress.
std::size_t count_planned_steps(const conf::BuildConfiguration& config,
                                const std::filesystem::path&    project_root,
                                const std::filesystem::path& build_dir, bld::BuildCache* cache,
                                conf::BuildOptions options)
{
  const auto contribution = [&](const bld::BuildTarget& target) -> std::size_t
  {
    if (cache != nullptr)
    {
      auto need = cache->needs_rebuild(target);
      if (need && !*need)
      {
        return 0;
      }
    }
    return target.source_files.size() + 1;
  };

  std::size_t total = 0;
  for (const auto& library : config.libraries)
  {
    total += contribution(plan_library_target(config, library, project_root, build_dir, options));
  }
  for (const auto& binary : config.binaries)
  {
    total += contribution(plan_executable_target("binary", binary.name, binary.sources, config,
                                                 binary.libraries, project_root, build_dir, options,
                                                 {}, {}));
  }
  if (conf::enabled(options.with_tests))
  {
    const auto tests_dir = build_dir / "tests";
    for (const auto& test : config.tests)
    {
      std::vector<std::string> test_link_flag_strings;
      test_link_flag_strings.reserve(test.link_flags.size());
      for (const auto& f : test.link_flags)
      {
        test_link_flag_strings.emplace_back(f.flag);
      }
      total +=
          contribution(plan_executable_target("test", test.name, test.sources, config,
                                              test.libraries, project_root, build_dir, options,
                                              test_link_flag_strings, tests_dir));
    }
  }
  return total;
}

}  // namespace

std::expected<int, std::string> executeBuild(conf::BuildOptions    options,
                                             const CommandContext& context) noexcept
{
  try
  {
    auto&      logger     = *context.logger;
    const auto wall_start = std::chrono::steady_clock::now();
    if (conf::enabled(options.verbose))
    {
      logger.set_verbose(true);
    }

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
    {
      cache = std::move(*c);
    }
    else
    {
      logger.warning("build cache unavailable: " + c.error());
    }

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

    const auto plural = [](std::size_t n, std::string_view singular, std::string_view plur)
    { return std::to_string(n) + " " + std::string(n == 1 ? singular : plur); };
    logger.info("Building project (" + plural(config.libraries.size(), "library", "libraries") +
                ", " + plural(config.binaries.size(), "binary", "binaries") + ", " +
                plural(config.tests.size(), "test", "tests") + ")");

    if (auto cc = conf::emit_compile_commands(config, context.projectRoot, build_dir, options))
    {
      logger.debug("wrote " + cc->string());
    }
    else
    {
      logger.warning("compile_commands.json: " + cc.error());
    }

    for (const auto& sp : config.subprojects)
    {
      auto r = run_external_subproject(sp, context.projectRoot / sp.path, logger);
      if (!r)
      {
        return std::unexpected(r.error());
      }
    }

    ProgressReporter progress;
    progress.set_total(
        count_planned_steps(config, context.projectRoot, build_dir, cache.get(), options));

    std::size_t built  = 0;
    std::size_t cached = 0;

    for (const auto& library : config.libraries)
    {
      auto r = build_library(config, library, context.projectRoot, build_dir, cache.get(), options,
                             logger, cached, progress);
      if (!r)
      {
        return std::unexpected(r.error());
      }
      ++built;
    }

    // Tests/binaries are independent and typically one source each — outer
    // parallelism across targets is a bigger win than inner per-source. Force
    // inner jobs=1 in the workers so we don't run jobs² g++ processes.
    conf::BuildOptions inner_opts = options;
    inner_opts.jobs               = 1;

    std::atomic<std::size_t> bin_cached{0};
    if (auto r = run_in_parallel(config.binaries, options.jobs,
                                 [&](const conf::Binary& binary) -> std::expected<void, std::string>
                                 {
                                   std::size_t local_cached = 0;
                                   auto        rr           = build_executable(
                                       "binary", binary.name, binary.sources, config,
                                       binary.libraries, context.projectRoot, build_dir,
                                       cache.get(), inner_opts, logger, local_cached, progress);
                                   if (!rr)
                                   {
                                     return std::unexpected(rr.error());
                                   }
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
    // Skipped unless `--with-tests` is set; `cppup test` re-enters with the
    // flag on before running the binaries.
    if (conf::enabled(options.with_tests))
    {
      const auto tests_dir = build_dir / "tests";
      if (!config.tests.empty())
      {
        std::filesystem::create_directories(tests_dir);
      }

      std::atomic<std::size_t> test_cached{0};
      if (auto r = run_in_parallel(
              config.tests, options.jobs,
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
                    "test", test.name, test.sources, config, test.libraries, context.projectRoot,
                    build_dir, cache.get(), inner_opts, logger, local_cached, progress,
                    test_link_flag_strings, tests_dir);
                if (!rr)
                {
                  return std::unexpected(rr.error());
                }
                test_cached.fetch_add(local_cached, std::memory_order_relaxed);
                return {};
              });
          !r)
      {
        return std::unexpected(r.error());
      }
      built += config.tests.size();
      cached += test_cached.load();
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

    const auto wall_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                             std::chrono::steady_clock::now() - wall_start)
                             .count();
    const auto format_wall = [](long long ms) -> std::string
    {
      if (ms < 1000)
      {
        return std::to_string(ms) + "ms";
      }
      const long long whole_sec = ms / 1000;
      const long long tenths    = (ms % 1000) / 100;
      if (whole_sec < 60)
      {
        return std::to_string(whole_sec) + "." + std::to_string(tenths) + "s";
      }
      return std::to_string(whole_sec / 60) + "m" + std::to_string(whole_sec % 60) + "s";
    };
    const std::size_t steps    = progress.done.load(std::memory_order_relaxed);
    const std::size_t rebuilt  = built - cached;
    std::string       summary  = "build complete: ";
    summary += std::to_string(rebuilt) + "/" + std::to_string(built) + " targets rebuilt";
    if (cached > 0)
    {
      summary += " (" + std::to_string(cached) + " cached)";
    }
    if (steps > 0)
    {
      summary += ", " + std::to_string(steps) + " compile/link steps";
    }
    summary += " in " + format_wall(wall_ms);
    logger.info(summary);
    if (cache != nullptr)
    {
      auto stats = cache->get_stats();
      if (stats)
      {
        logger.info("cache hit rate: " + std::to_string(static_cast<int>(stats->hit_rate * 100.0)) +
                    "%");
      }
    }
    return 0;
  }
  catch (const std::exception& e)
  {
    return std::unexpected(std::string{"build failed: "} + e.what());
  }
}

}  // namespace cppup::cli
