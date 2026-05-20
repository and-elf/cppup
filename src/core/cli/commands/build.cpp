#include <atomic>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
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
#include "../../logger/console/console_logger.hpp"
#include "build_options.hpp"
#include "commands.hpp"
#include "common.h"
#include "embedded_configuration_header.hpp"

namespace cppup::cli
{

namespace
{

namespace conf = cppup::configuration;
namespace bld  = cppup::build;

// Project/build directory pair carried through the build pipeline. Bundled
// so signatures don't end up with two adjacent fs::path parameters that are
// easy to swap by mistake.
struct BuildPaths
{
  std::filesystem::path project_root;
  std::filesystem::path build_dir;
};

void append_common_flags(std::vector<std::string>& out, const conf::BuildConfiguration& config,
                         const std::filesystem::path& project_root, conf::BuildOptions options)
{
  if (config.toolchain)
  {
    for (auto& f : conf::dialect_flags(*config.toolchain))
    {
      out.emplace_back(std::move(f));
    }
  }
  for (const auto& flag : config.compile_flags)
  {
    out.emplace_back(flag.flag);
  }
  for (const auto& def : config.definitions)
  {
    std::string d = "-D" + std::string(def.name);
    if (!def.value.empty())
    {
      d += "=" + std::string(def.value);
    }
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
  if (!cache)
  {
    return deps;
  }

  for (const auto& source : sources)
  {
    if (!std::filesystem::exists(source))
    {
      continue;
    }
    bld::FileDependency dep;
    dep.file_path     = source;
    dep.last_modified = std::filesystem::last_write_time(source);
    auto checksum     = cache->calculate_file_checksum(source);
    if (checksum)
    {
      dep.checksum = *checksum;
    }

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
    const auto             n = done.fetch_add(1, std::memory_order_relaxed) + 1;
    const std::scoped_lock lk(print_mu);
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
  if (items.empty())
  {
    return {};
  }

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
        const std::scoped_lock lk(err_mu);
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
      for (const auto& f : flags)
      {
        cmd << ' ' << f;
      }
      cmd << ' ' << t.source.string() << " -o " << t.object.string();
      {
        const std::scoped_lock lk(log_mu);
        logger.debug("compile: " + cmd.str());
      }
      if (std::system(cmd.str().c_str()) != 0)
      {
        const std::scoped_lock lk(err_mu);
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

bld::BuildTarget make_library_target(const conf::BuildConfiguration& config,
                                     const conf::Library& library, const BuildPaths& paths,
                                     conf::BuildOptions options);

std::expected<std::filesystem::path, std::string> build_library(
    const conf::BuildConfiguration& config, const conf::Library& library, const BuildPaths& paths,
    bld::BuildCache* cache, conf::BuildOptions options, Logger& logger, std::size_t& cached_counter,
    ProgressReporter& progress)
{
  auto target = make_library_target(config, library, paths, options);

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
    auto obj = paths.build_dir / (src.stem().string() + "_" + library.name + ".o");
    lib_tasks.push_back({src, obj});
    objects.push_back(obj);
  }
  if (auto rc = compile_objects_parallel("g++", lib_tasks, target.compile_flags, options.jobs,
                                         logger, &progress);
      !rc)
  {
    return std::unexpected(rc.error());
  }

  std::ostringstream ar_cmd;
  ar_cmd << "ar rcs " << target.output_path.string();
  for (const auto& obj : objects)
  {
    ar_cmd << ' ' << obj.string();
  }
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

// The two outputs of resolve_link_set + aggregate_link_flags, bundled so they
// move together and don't sit as two adjacent vector<string> in signatures.
struct ResolvedLinks
{
  std::vector<std::string> libs;
  std::vector<std::string> library_flags;
};

// Composes the link command for executables: -L<build_dir>, resolved internal
// libs wrapped in --start-group/--end-group, aggregated link_flags from those
// libraries, global config link_flags, the target's own extra_link_flags, and
// asan/coverage. Used by both the real builder and the planning pass; the
// cache key depends on link_flags so the two must compose identically.
std::vector<std::string> compose_link_flags(const ResolvedLinks&            resolved,
                                            const conf::BuildConfiguration& config,
                                            const std::vector<std::string>& extra_link_flags,
                                            conf::BuildOptions              options,
                                            const std::filesystem::path&    build_dir)
{
  std::vector<std::string> link_flags;
  link_flags.push_back("-L" + build_dir.string());
  if (!resolved.libs.empty())
  {
    link_flags.emplace_back("-Wl,--start-group");
    for (const auto& lib : resolved.libs)
    {
      link_flags.push_back("-l" + lib);
    }
    link_flags.emplace_back("-Wl,--end-group");
  }
  for (const auto& f : resolved.library_flags)
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
  return link_flags;
}

// Factory: build the BuildTarget for a library. Shared between the real
// builder and the planning pass so cache keys can't drift out of sync.
bld::BuildTarget make_library_target(const conf::BuildConfiguration& config,
                                     const conf::Library& library, const BuildPaths& paths,
                                     conf::BuildOptions options)
{
  bld::BuildTarget target;
  target.name        = library.name;
  target.type        = "library";
  const char* ext    = (library.type == conf::LibraryType::Static) ? ".a" : ".so";
  target.output_path = paths.build_dir / ("lib" + library.name + ext);
  for (const auto& src : library.sources)
  {
    target.source_files.push_back(paths.project_root / src);
  }
  append_common_flags(target.compile_flags, config, paths.project_root, options);
  for (const auto& inc : config.include_paths)
  {
    target.include_paths.push_back(paths.project_root / inc);
  }
  return target;
}

// Factory: build the BuildTarget for a binary or test. Returns `unexpected`
// if linked_library_names can't be resolved — the planner ignores that error
// (treats the target as one to-be-rebuilt), the real builder propagates it.
std::expected<bld::BuildTarget, std::string> make_executable_target(
    const std::string& kind, const std::string& name, const std::vector<std::string>& sources,
    const conf::BuildConfiguration& config, const std::vector<std::string>& linked_library_names,
    const BuildPaths& paths, conf::BuildOptions options,
    const std::vector<std::string>& extra_link_flags, const std::filesystem::path& output_dir)
{
  const auto& target_dir = output_dir.empty() ? paths.build_dir : output_dir;

  bld::BuildTarget target;
  target.name        = name;
  target.type        = kind;
  target.output_path = target_dir / name;
  for (const auto& src : sources)
  {
    target.source_files.push_back(paths.project_root / src);
  }
  append_common_flags(target.compile_flags, config, paths.project_root, options);
  for (const auto& inc : config.include_paths)
  {
    target.include_paths.push_back(paths.project_root / inc);
  }

  auto resolved = conf::resolve_link_set(linked_library_names, config.libraries);
  if (!resolved)
  {
    return std::unexpected(std::move(resolved).error());
  }
  ResolvedLinks links{.libs          = *resolved,
                      .library_flags = conf::aggregate_link_flags(*resolved, config.libraries)};
  target.link_flags = compose_link_flags(links, config, extra_link_flags, options, paths.build_dir);
  return target;
}

std::expected<void, std::string> build_executable(
    const std::string& kind, const std::string& name, const std::vector<std::string>& sources,
    const conf::BuildConfiguration& config, const std::vector<std::string>& linked_library_names,
    const BuildPaths& paths, bld::BuildCache* cache, conf::BuildOptions options, Logger& logger,
    std::size_t& cached_counter, ProgressReporter& progress,
    const std::vector<std::string>& extra_link_flags = {},
    const std::filesystem::path&    output_dir       = {})
{
  auto target = make_executable_target(kind, name, sources, config, linked_library_names, paths,
                                       options, extra_link_flags, output_dir);
  if (!target)
  {
    return std::unexpected(kind + " " + name + ": " + target.error());
  }

  if (cache != nullptr)
  {
    auto need = cache->needs_rebuild(*target);
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
  bin_tasks.reserve(target->source_files.size());
  bin_objects.reserve(target->source_files.size());
  for (const auto& src : target->source_files)
  {
    auto obj = paths.build_dir / (src.stem().string() + "_" + name + "_" + kind + ".o");
    bin_tasks.push_back({src, obj});
    bin_objects.push_back(obj);
  }
  if (auto rc = compile_objects_parallel(compiler, bin_tasks, target->compile_flags, options.jobs,
                                         logger, &progress);
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
  for (const auto& f : target->link_flags)
  {
    cmd << ' ' << f;
  }
  cmd << " -o " << target->output_path.string();

  logger.debug("link: " + cmd.str());
  if (std::system(cmd.str().c_str()) != 0)
  {
    return std::unexpected(kind + " link failed: " + name);
  }
  progress.step("linking", target->output_path.filename().string());

  if (cache != nullptr)
  {
    auto deps = collect_dependencies(cache, target->source_files, target->include_paths);
    return cache->cache_build_result(*target, deps);
  }
  return {};
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
std::size_t count_planned_steps(const conf::BuildConfiguration& config, const BuildPaths& paths,
                                bld::BuildCache* cache, conf::BuildOptions options)
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
  // If the executable factory fails (unresolved link set), assume it'll
  // need a full rebuild's worth of steps — the real builder will surface
  // the error later.
  const auto exec_contribution =
      [&](const std::string& kind, const std::string& name, const std::vector<std::string>& sources,
          const std::vector<std::string>& libraries, const std::vector<std::string>& extra,
          const std::filesystem::path& output_dir) -> std::size_t
  {
    auto t = make_executable_target(kind, name, sources, config, libraries, paths, options, extra,
                                    output_dir);
    return t ? contribution(*t) : sources.size() + 1;
  };

  std::size_t total = 0;
  for (const auto& library : config.libraries)
  {
    total += contribution(make_library_target(config, library, paths, options));
  }
  for (const auto& binary : config.binaries)
  {
    total += exec_contribution("binary", binary.name, binary.sources, binary.libraries, {}, {});
  }
  if (conf::enabled(options.with_tests))
  {
    const auto tests_dir = paths.build_dir / "tests";
    for (const auto& test : config.tests)
    {
      std::vector<std::string> test_link_flag_strings;
      test_link_flag_strings.reserve(test.link_flags.size());
      for (const auto& f : test.link_flags)
      {
        test_link_flag_strings.emplace_back(f.flag);
      }
      total += exec_contribution("test", test.name, test.sources, test.libraries,
                                 test_link_flag_strings, tests_dir);
    }
  }
  return total;
}

// Write .cppup/include/cppup/configuration.hpp from the bytes #embed-ed into
// the cppup binary at compile time. Idempotent: skip the write when on-disk
// contents already match, so `cppup build` doesn't churn the file's mtime
// on every invocation.
void materialize_configuration_header(const std::filesystem::path& cppup_dir)
{
  const auto header_dir  = cppup_dir / "include" / "cppup";
  const auto header_path = header_dir / "configuration.hpp";
  std::filesystem::create_directories(header_dir);

  if (std::filesystem::exists(header_path))
  {
    std::ifstream const in(header_path, std::ios::binary);
    std::stringstream   buf;
    buf << in.rdbuf();
    if (buf.str() == kConfigurationHeader)
    {
      return;
    }
  }
  std::ofstream out(header_path, std::ios::binary | std::ios::trunc);
  out.write(kConfigurationHeader.data(), static_cast<std::streamsize>(kConfigurationHeader.size()));
}

std::expected<conf::BuildConfiguration, std::string> load_build_configuration(
    const std::filesystem::path& project_root, const std::filesystem::path& cppup_dir)
{
  conf::CompilerOptions compiler_opts;
  compiler_opts.include_paths.push_back((cppup_dir / "include").string());
  compiler_opts.include_paths.push_back((project_root / "include").string());
  compiler_opts.include_paths.push_back((project_root / "src").string());
  compiler_opts.output_directory = (cppup_dir / "build" / "config").string();

  conf::ConfigurationCompiler compiler(std::move(compiler_opts));
  auto                        config_result = conf::load_with_subprojects(project_root, compiler);
  if (!config_result)
  {
    return std::unexpected("load build configuration failed: " + config_result.error());
  }
  return *config_result;
}

std::string format_project_summary(const conf::BuildConfiguration& config)
{
  const auto plural = [](std::size_t n, std::string_view singular, std::string_view plur)
  { return std::to_string(n) + " " + std::string(n == 1 ? singular : plur); };
  return "Building project (" + plural(config.libraries.size(), "library", "libraries") + ", " +
         plural(config.binaries.size(), "binary", "binaries") + ", " +
         plural(config.tests.size(), "test", "tests") + ")";
}

std::expected<std::size_t, std::string> build_binaries_parallel(
    const conf::BuildConfiguration& config, const BuildPaths& paths, bld::BuildCache* cache,
    conf::BuildOptions options, Logger& logger, ProgressReporter& progress)
{
  std::atomic<std::size_t> total_cached{0};
  auto                     r = run_in_parallel(config.binaries, options.jobs,
                                               [&](const conf::Binary& binary) -> std::expected<void, std::string>
                                               {
                             std::size_t local_cached = 0;
                             auto rr = build_executable("binary", binary.name, binary.sources,
                                                                            config, binary.libraries, paths, cache,
                                                                            options, logger, local_cached, progress);
                             if (!rr)
                             {
                               return std::unexpected(rr.error());
                             }
                             total_cached.fetch_add(local_cached, std::memory_order_relaxed);
                             return {};
                           });
  if (!r)
  {
    return std::unexpected(r.error());
  }
  return total_cached.load();
}

// Build tests in parallel. Tests link only the internal libraries they name
// in Test::libraries plus verbatim flags in Test::link_flags (e.g.
// "-lgtest -lgtest_main -lpthread"). Binaries land in build/tests/ so both
// the test runner and VSCode testMate's glob find them.
std::expected<std::size_t, std::string> build_tests_parallel(
    const conf::BuildConfiguration& config, const BuildPaths& paths, bld::BuildCache* cache,
    conf::BuildOptions options, Logger& logger, ProgressReporter& progress)
{
  const auto tests_dir = paths.build_dir / "tests";
  if (!config.tests.empty())
  {
    std::filesystem::create_directories(tests_dir);
  }

  std::atomic<std::size_t> total_cached{0};
  auto                     r = run_in_parallel(config.tests, options.jobs,
                                               [&](const conf::Test& test) -> std::expected<void, std::string>
                                               {
                             std::vector<std::string> test_link_flag_strings;
                             test_link_flag_strings.reserve(test.link_flags.size());
                             for (const auto& f : test.link_flags)
                             {
                               test_link_flag_strings.emplace_back(f.flag);
                             }
                             std::size_t local_cached = 0;
                             auto rr = build_executable("test", test.name, test.sources, config,
                                                                            test.libraries, paths, cache, options,
                                                                            logger, local_cached, progress,
                                                                            test_link_flag_strings, tests_dir);
                             if (!rr)
                             {
                               return std::unexpected(rr.error());
                             }
                             total_cached.fetch_add(local_cached, std::memory_order_relaxed);
                             return {};
                           });
  if (!r)
  {
    return std::unexpected(r.error());
  }
  return total_cached.load();
}

std::string format_wall_time(long long ms)
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
}

struct BuildCounters
{
  std::size_t built  = 0;
  std::size_t cached = 0;
  std::size_t steps  = 0;
};

std::string build_summary_line(const BuildCounters& counts, long long wall_ms)
{
  const std::size_t rebuilt = counts.built - counts.cached;
  std::string       summary = "build complete: ";
  summary += std::to_string(rebuilt) + "/" + std::to_string(counts.built) + " targets rebuilt";
  if (counts.cached > 0)
  {
    summary += " (" + std::to_string(counts.cached) + " cached)";
  }
  if (counts.steps > 0)
  {
    summary += ", " + std::to_string(counts.steps) + " compile/link steps";
  }
  summary += " in " + format_wall_time(wall_ms);
  return summary;
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
      cppup::logger::console::ConsoleLogger::setGlobalConfig(
          {.defaultLevel = cppup::logger::LogLevel::Debug, .categoryOverrides = {}});
    }

    if (!std::filesystem::exists(context.projectRoot / "build.cpp"))
    {
      return std::unexpected("No build.cpp found in: " + context.projectRoot.string());
    }

    const auto cppup_dir = context.projectRoot / ".cppup";
    const auto build_dir = context.projectRoot / "build";
    std::filesystem::create_directories(build_dir);
    const BuildPaths paths{.project_root = context.projectRoot, .build_dir = build_dir};

    std::unique_ptr<bld::BuildCache> cache;
    if (auto c = bld::create_build_cache(cppup_dir / "cache", nullptr))
    {
      cache = std::move(*c);
    }
    else
    {
      logger.warning("build cache unavailable: " + c.error());
    }

    materialize_configuration_header(cppup_dir);

    auto config_result = load_build_configuration(context.projectRoot, cppup_dir);
    if (!config_result)
    {
      return std::unexpected(std::move(config_result).error());
    }
    const auto& config = *config_result;

    logger.info(format_project_summary(config));

    if (auto cc = conf::emit_compile_commands(config, paths.project_root, paths.build_dir, options))
    {
      logger.debug("wrote " + cc->string());
    }
    else
    {
      logger.warning("compile_commands.json: " + cc.error());
    }

    for (const auto& sp : config.subprojects)
    {
      auto r = run_external_subproject(sp, paths.project_root / sp.path, logger);
      if (!r)
      {
        return std::unexpected(r.error());
      }
    }

    ProgressReporter progress;
    progress.set_total(count_planned_steps(config, paths, cache.get(), options));

    BuildCounters counts;

    for (const auto& library : config.libraries)
    {
      auto r = build_library(config, library, paths, cache.get(), options, logger, counts.cached,
                             progress);
      if (!r)
      {
        return std::unexpected(r.error());
      }
      ++counts.built;
    }

    // Tests/binaries are independent and typically one source each — outer
    // parallelism across targets is a bigger win than inner per-source. Force
    // inner jobs=1 in the workers so we don't run jobs² g++ processes.
    conf::BuildOptions inner_opts = options;
    inner_opts.jobs               = 1;

    auto bin_cached =
        build_binaries_parallel(config, paths, cache.get(), inner_opts, logger, progress);
    if (!bin_cached)
    {
      return std::unexpected(std::move(bin_cached).error());
    }
    counts.built += config.binaries.size();
    counts.cached += *bin_cached;

    if (conf::enabled(options.with_tests))
    {
      auto test_cached =
          build_tests_parallel(config, paths, cache.get(), inner_opts, logger, progress);
      if (!test_cached)
      {
        return std::unexpected(std::move(test_cached).error());
      }
      counts.built += config.tests.size();
      counts.cached += *test_cached;
    }

    if (!config.build_steps.empty())
    {
      conf::BuildStepExecutor const executor;
      auto                          step_result = executor.execute_build_steps(config);
      if (!step_result.success)
      {
        return std::unexpected("build step failed: " + step_result.error_message);
      }
    }

    const auto wall_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                             std::chrono::steady_clock::now() - wall_start)
                             .count();
    counts.steps = progress.done.load(std::memory_order_relaxed);
    logger.info(build_summary_line(counts, wall_ms));
    if (cache != nullptr)
    {
      if (auto stats = cache->get_stats())
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
