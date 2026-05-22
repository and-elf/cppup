#include <atomic>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "../../build/cache.hpp"
#include "../../configuration/build_configuration.hpp"
#include "../../configuration/build_step_executor.hpp"
#include "../../configuration/compile_commands.hpp"
#include "../../configuration/compiler.hpp"
#include "../../configuration/link_resolution.hpp"
#include "../../configuration/subproject_loader.hpp"
#include "../../configuration/toolchain_flags.hpp"
#include "../../logger/console/console_logger.hpp"
#include "build_options.hpp"
#include "command_context.hpp"
#include "commands.hpp"
#include "core/panic.hpp"
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

// Take the whole BuildTarget rather than (sources, include_paths) so the
// adjacent-same-type swap trap is gone and callers don't have to remember
// which vector goes first.
std::vector<bld::FileDependency> collect_dependencies(bld::BuildCache&        cache,
                                                      const bld::BuildTarget& target)
{
  std::vector<bld::FileDependency> deps;
  for (const auto& source : target.source_files)
  {
    if (!std::filesystem::exists(source))
    {
      continue;
    }
    bld::FileDependency dep;
    dep.file_path     = source;
    dep.last_modified = std::filesystem::last_write_time(source);
    if (auto checksum = cache.calculate_file_checksum(source))
    {
      dep.checksum = *checksum;
    }

    for (const auto& inc : bld::DependencyScanner::scan_includes(source))
    {
      for (const auto& dir : target.include_paths)
      {
        auto resolved = dir / inc;
        if (std::filesystem::exists(resolved))
        {
          dep.includes.push_back(resolved);
          break;
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
    std::cout << '[' << std::setw(width) << n << '/' << total << "] " << action << ' ' << name
              << '\n';
  }
};

// Run `work` over each item in `items` across up to `jobs` worker threads.
// `jobs == 0` means auto. On the first exception from `work`, the abort flag
// short-circuits remaining workers; in-flight items complete naturally (we
// don't try to cancel). After join, the orchestrator rethrows the first
// failure as runtime_error. Used to parallelize the outer per-target build
// loops (tests, binaries).
template <typename Item, typename Work>
void run_in_parallel(const std::vector<Item>& items, unsigned jobs, Work work)
{
  if (items.empty())
  {
    return;
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
      try
      {
        work(items[i]);
      }
      catch (const std::exception& e)
      {
        const std::scoped_lock lk(err_mu);
        if (!aborted.exchange(true))
        {
          first_err = e.what();
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
    throw std::runtime_error(first_err);
  }
}

// Compile a batch of sources concurrently. `jobs == 0` means auto (hardware
// concurrency). On the first compilation failure the abort flag short-circuits
// remaining work; in-flight compilations finish naturally (we don't try to
// kill child processes). The logger is serialized so debug lines don't
// interleave. Throws runtime_error with the first failure message after join.
void compile_objects_parallel(const std::string& compiler, const std::vector<CompileTask>& tasks,
                              const std::vector<std::string>& flags, unsigned jobs, Logger& logger,
                              ProgressReporter* progress)
{
  if (tasks.empty())
  {
    return;
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
    throw std::runtime_error(first_err);
  }
}

// Shared environment for the build pipeline. Non-owning pointers (not refs)
// so the struct is trivially copyable — the inner-jobs override (force
// per-source jobs=1 while parallelizing across targets) is done by copying a
// BuildContext and mutating the copy's value-typed `options`. All pointers
// are required to be non-null; constructed once in executeBuild and threaded
// through the per-target functions.
struct BuildContext
{
  const conf::BuildConfiguration* config;
  const BuildPaths*               paths;
  bld::BuildCache*                cache;
  conf::BuildOptions              options;
  Logger*                         logger;
  ProgressReporter*               progress;
};

// Per-executable build inputs. Spans are non-owning views into vectors that
// outlive the spec (the source Binary/Test/local vector lives in the caller's
// stack frame). `kind` is "binary" or "test" — used in target.type and in
// object filenames to keep binary and test objects from colliding.
struct ExecutableSpec
{
  std::string                  kind;
  std::string                  name;
  std::span<const std::string> sources;
  std::span<const std::string> linked_library_names;
  std::span<const std::string> extra_link_flags = {};
  std::filesystem::path        output_dir       = {};
};

bld::BuildTarget make_library_target(const conf::Library& library, const BuildContext& ctx);

// Returns true if the library was served from cache (no compile/archive done).
bool build_library(const conf::Library& library, const BuildContext& ctx)
{
  auto target = make_library_target(library, ctx);

  if (!ctx.cache->needs_rebuild(target))
  {
    ctx.logger->info("cached library: " + library.name);
    return true;
  }

  std::vector<CompileTask>           lib_tasks;
  std::vector<std::filesystem::path> objects;
  lib_tasks.reserve(target.source_files.size());
  objects.reserve(target.source_files.size());
  for (const auto& src : target.source_files)
  {
    auto obj = ctx.paths->build_dir / (src.stem().string() + "_" + library.name + ".o");
    lib_tasks.push_back({src, obj});
    objects.push_back(obj);
  }
  compile_objects_parallel("g++", lib_tasks, target.compile_flags, ctx.options.jobs, *ctx.logger,
                           ctx.progress);

  std::ostringstream ar_cmd;
  ar_cmd << "ar rcs " << target.output_path.string();
  for (const auto& obj : objects)
  {
    ar_cmd << ' ' << obj.string();
  }
  ctx.logger->debug("archive: " + ar_cmd.str());
  if (std::system(ar_cmd.str().c_str()) != 0)
  {
    throw std::runtime_error("archive failed: " + library.name);
  }
  ctx.progress->step("archiving", target.output_path.filename().string());

  auto deps = collect_dependencies(*ctx.cache, target);
  ctx.cache->cache_build_result(target, deps);
  return false;
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
                                            std::span<const std::string>    extra_link_flags,
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
bld::BuildTarget make_library_target(const conf::Library& library, const BuildContext& ctx)
{
  const auto& config = *ctx.config;
  const auto& paths  = *ctx.paths;

  bld::BuildTarget target;
  target.name        = library.name;
  target.type        = "library";
  const char* ext    = (library.type == conf::LibraryType::Static) ? ".a" : ".so";
  target.output_path = paths.build_dir / ("lib" + library.name + ext);
  for (const auto& src : library.sources)
  {
    target.source_files.push_back(paths.project_root / src);
  }
  append_common_flags(target.compile_flags, config, paths.project_root, ctx.options);
  for (const auto& inc : config.include_paths)
  {
    target.include_paths.push_back(paths.project_root / inc);
  }
  return target;
}

// Factory: build the BuildTarget for a binary or test. Throws runtime_error
// if spec.linked_library_names can't be resolved. The planner catches and
// treats the target as needing a full rebuild's worth of steps; the real
// builder lets the throw propagate to the executeBuild boundary.
bld::BuildTarget make_executable_target(const ExecutableSpec& spec, const BuildContext& ctx)
{
  const auto& config     = *ctx.config;
  const auto& paths      = *ctx.paths;
  const auto& target_dir = spec.output_dir.empty() ? paths.build_dir : spec.output_dir;

  bld::BuildTarget target;
  target.name        = spec.name;
  target.type        = spec.kind;
  target.output_path = target_dir / spec.name;
  for (const auto& src : spec.sources)
  {
    target.source_files.push_back(paths.project_root / src);
  }
  append_common_flags(target.compile_flags, config, paths.project_root, ctx.options);
  for (const auto& inc : config.include_paths)
  {
    target.include_paths.push_back(paths.project_root / inc);
  }

  const std::vector<std::string> link_roots(spec.linked_library_names.begin(),
                                            spec.linked_library_names.end());
  auto                           resolved = conf::resolve_link_set(link_roots, config.libraries);
  if (!resolved)
  {
    throw std::runtime_error(spec.kind + " " + spec.name + ": " + std::move(resolved).error());
  }
  const ResolvedLinks links{
      .libs = *resolved, .library_flags = conf::aggregate_link_flags(*resolved, config.libraries)};
  target.link_flags =
      compose_link_flags(links, config, spec.extra_link_flags, ctx.options, paths.build_dir);
  return target;
}

// Returns true if the executable was served from cache (no compile/link done).
bool build_executable(const ExecutableSpec& spec, const BuildContext& ctx)
{
  auto target = make_executable_target(spec, ctx);

  if (!ctx.cache->needs_rebuild(target))
  {
    ctx.logger->info("cached " + spec.kind + ": " + spec.name);
    return true;
  }

  const std::string compiler =
      ctx.config->toolchain ? std::string(ctx.config->toolchain->name) : "g++";

  // Split compile and link so per-source compilation can run in parallel under
  // -j. Object filenames include kind+name to avoid colliding with library
  // objects or other binaries that share a source basename.
  std::vector<CompileTask>           bin_tasks;
  std::vector<std::filesystem::path> bin_objects;
  bin_tasks.reserve(target.source_files.size());
  bin_objects.reserve(target.source_files.size());
  for (const auto& src : target.source_files)
  {
    auto obj =
        ctx.paths->build_dir / (src.stem().string() + "_" + spec.name + "_" + spec.kind + ".o");
    bin_tasks.push_back({src, obj});
    bin_objects.push_back(obj);
  }
  compile_objects_parallel(compiler, bin_tasks, target.compile_flags, ctx.options.jobs, *ctx.logger,
                           ctx.progress);

  std::ostringstream cmd;
  cmd << compiler;
  for (const auto& obj : bin_objects)
  {
    cmd << ' ' << obj.string();
  }
  for (const auto& f : target.link_flags)
  {
    cmd << ' ' << f;
  }
  cmd << " -o " << target.output_path.string();

  ctx.logger->debug("link: " + cmd.str());
  if (std::system(cmd.str().c_str()) != 0)
  {
    throw std::runtime_error(spec.kind + " link failed: " + spec.name);
  }
  ctx.progress->step("linking", target.output_path.filename().string());

  auto deps = collect_dependencies(*ctx.cache, target);
  ctx.cache->cache_build_result(target, deps);
  return false;
}

// Hand off a non-Cppup subproject to its native build system. The external
// build's stdout/stderr flows through directly — we don't wrap it in
// progress reporting. Throws runtime_error on external-tool failure.
void run_external_subproject(const conf::Subproject& sp, const std::filesystem::path& sp_dir,
                             Logger& logger)
{
  if (!sp.build_system)
  {
    return;
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
        throw std::runtime_error("subproject " + sp.path + ": cmake configure failed");
      }
      std::ostringstream bld_cmd;
      bld_cmd << "cmake --build " << (sp_dir / "build").string();
      if (std::system(bld_cmd.str().c_str()) != 0)
      {
        throw std::runtime_error("subproject " + sp.path + ": cmake build failed");
      }
      return;
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
        throw std::runtime_error("subproject " + sp.path + ": make failed");
      }
      return;
    }
    case conf::BuildSystem::HeaderOnly:
      logger.info("Using header-only subproject " + sp.path);
      return;
    case conf::BuildSystem::Cppup:
      // Cppup subprojects were already merged in load_with_subprojects;
      // they should not appear here.
      return;
  }
}

// Walk libraries/binaries/tests, query the cache, and return the number of
// progress steps the build will emit: compile-per-source plus one finalize
// step (archive for libraries, link for binaries/tests). Cached targets are
// skipped — they emit a single "cached X" line instead, outside progress.
std::size_t count_planned_steps(const BuildContext& ctx)
{
  const auto contribution = [&](const bld::BuildTarget& target) -> std::size_t
  {
    if (!ctx.cache->needs_rebuild(target))
    {
      return 0;
    }
    return target.source_files.size() + 1;
  };
  // If the executable factory throws (unresolved link set), assume it'll
  // need a full rebuild's worth of steps — the real builder will surface
  // the error later.
  const auto exec_contribution = [&](const ExecutableSpec& spec) -> std::size_t
  {
    try
    {
      return contribution(make_executable_target(spec, ctx));
    }
    catch (const std::exception&)
    {
      return spec.sources.size() + 1;
    }
  };

  std::size_t total = 0;
  for (const auto& library : ctx.config->libraries)
  {
    total += contribution(make_library_target(library, ctx));
  }
  for (const auto& binary : ctx.config->binaries)
  {
    total += exec_contribution({
        .kind                 = "binary",
        .name                 = binary.name,
        .sources              = binary.sources,
        .linked_library_names = binary.libraries,
    });
  }
  if (conf::enabled(ctx.options.with_tests))
  {
    const auto tests_dir = ctx.paths->build_dir / "tests";
    for (const auto& test : ctx.config->tests)
    {
      std::vector<std::string> test_link_flag_strings;
      test_link_flag_strings.reserve(test.link_flags.size());
      for (const auto& f : test.link_flags)
      {
        test_link_flag_strings.emplace_back(f.flag);
      }
      total += exec_contribution({
          .kind                 = "test",
          .name                 = test.name,
          .sources              = test.sources,
          .linked_library_names = test.libraries,
          .extra_link_flags     = test_link_flag_strings,
          .output_dir           = tests_dir,
      });
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

conf::BuildConfiguration load_build_configuration(const std::filesystem::path& project_root,
                                                  const std::filesystem::path& cppup_dir)
{
  conf::CompilerOptions compiler_opts;
  compiler_opts.include_paths.push_back((cppup_dir / "include").string());
  compiler_opts.include_paths.push_back((project_root / "include").string());
  compiler_opts.include_paths.push_back((project_root / "src").string());
  compiler_opts.output_directory = (cppup_dir / "build" / "config").string();

  conf::ConfigurationCompiler compiler(std::move(compiler_opts));
  auto                        config_result = conf::load_with_subprojects(project_root, compiler);
  CPPUP_CHECK(config_result.has_value(), "configuration compilation failed");
  return *config_result;
}

std::string format_project_summary(const conf::BuildConfiguration& config)
{
  const auto plural = [](std::size_t n, std::string_view singular, std::string_view plur)
  { return std::format("{} {}{}", n, singular, (n == 1) ? "" : plur); };
  return std::format("Building project ({}, {}, {})",
                     plural(config.libraries.size(), "library", "libraries"),
                     plural(config.binaries.size(), "binary", "binaries"),
                     plural(config.tests.size(), "test", "tests"));
}

std::size_t build_binaries_parallel(const BuildContext& ctx)
{
  std::atomic<std::size_t> total_cached{0};
  run_in_parallel(ctx.config->binaries, ctx.options.jobs,
                  [&](const conf::Binary& binary)
                  {
                    const bool cached = build_executable(
                        {
                            .kind                 = "binary",
                            .name                 = binary.name,
                            .sources              = binary.sources,
                            .linked_library_names = binary.libraries,
                        },
                        ctx);
                    if (cached)
                    {
                      total_cached.fetch_add(1, std::memory_order_relaxed);
                    }
                  });
  return total_cached.load();
}

// Build tests in parallel. Tests link only the internal libraries they name
// in Test::libraries plus verbatim flags in Test::link_flags (e.g.
// "-lgtest -lgtest_main -lpthread"). Binaries land in build/tests/ so both
// the test runner and VSCode testMate's glob find them.
std::size_t build_tests_parallel(const BuildContext& ctx)
{
  const auto tests_dir = ctx.paths->build_dir / "tests";
  if (!ctx.config->tests.empty())
  {
    std::filesystem::create_directories(tests_dir);
  }

  std::atomic<std::size_t> total_cached{0};
  run_in_parallel(ctx.config->tests, ctx.options.jobs,
                  [&](const conf::Test& test)
                  {
                    std::vector<std::string> test_link_flag_strings;
                    test_link_flag_strings.reserve(test.link_flags.size());
                    for (const auto& f : test.link_flags)
                    {
                      test_link_flag_strings.emplace_back(f.flag);
                    }
                    const bool cached = build_executable(
                        {
                            .kind                 = "test",
                            .name                 = test.name,
                            .sources              = test.sources,
                            .linked_library_names = test.libraries,
                            .extra_link_flags     = test_link_flag_strings,
                            .output_dir           = tests_dir,
                        },
                        ctx);
                    if (cached)
                    {
                      total_cached.fetch_add(1, std::memory_order_relaxed);
                    }
                  });
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

    auto cache = bld::create_build_cache(cppup_dir / "cache", nullptr);
    if (!cache)
    {
      logger.warning("build cache unavailable");
    }

    materialize_configuration_header(cppup_dir);

    const auto config = load_build_configuration(context.projectRoot, cppup_dir);

    logger.info(format_project_summary(config));

    logger.debug(
        "wrote " +
        conf::emit_compile_commands(config, paths.project_root, paths.build_dir, options).string());

    for (const auto& sp : config.subprojects)
    {
      run_external_subproject(sp, paths.project_root / sp.path, logger);
    }

    ProgressReporter   progress;
    const BuildContext ctx{
        .config   = &config,
        .paths    = &paths,
        .cache    = cache.get(),
        .options  = options,
        .logger   = &logger,
        .progress = &progress,
    };
    progress.set_total(count_planned_steps(ctx));

    BuildCounters counts;

    for (const auto& library : config.libraries)
    {
      if (build_library(library, ctx))
      {
        ++counts.cached;
      }
      ++counts.built;
    }

    // Tests/binaries are independent and typically one source each — outer
    // parallelism across targets is a bigger win than inner per-source. Force
    // inner jobs=1 in the workers so we don't run jobs² g++ processes.
    BuildContext inner_ctx = ctx;
    inner_ctx.options.jobs = 1;

    counts.cached += build_binaries_parallel(inner_ctx);
    counts.built += config.binaries.size();

    if (conf::enabled(options.with_tests))
    {
      counts.cached += build_tests_parallel(inner_ctx);
      counts.built += config.tests.size();
    }

    if (!config.build_steps.empty())
    {
      auto step_result = conf::BuildStepExecutor::execute_build_steps(config);
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
      logger.info("cache hit rate: " +
                  std::to_string(static_cast<int>(cache->get_stats().hit_rate * 100.0)) + "%");
    }
    return 0;
  }
  catch (const std::exception& e)
  {
    return std::unexpected(e.what());
  }
  catch (...)
  {
    return std::unexpected("build failed with unknown error");
  }
}

}  // namespace cppup::cli
