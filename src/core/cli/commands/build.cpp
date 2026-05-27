#include <atomic>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
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
#include "../../configuration/platform.hpp"
#include "../../configuration/subproject_loader.hpp"
#include "../../configuration/toolchain_flags.hpp"
#include "../../logger/console/console_logger.hpp"
#include "../../plugin/static_registry.hpp"
#include "../../plugin/test_framework_plugin.hpp"
#include "build_options.hpp"
#include "command_context.hpp"
#include "commands.hpp"
#include "core/panic.hpp"
#include "embedded_configuration_header.hpp"
#include "selection_resolver.hpp"
#include "subproject_runner.hpp"

namespace cppup::cli
{

namespace
{

namespace conf = cppup::configuration;

namespace bld = cppup::build;

std::string format_command_for_log(const std::string& command, const std::vector<std::string>& args)
{
  std::ostringstream cmd;
  cmd << command;
  for (const auto& arg : args)
  {
    cmd << ' ' << arg;
  }
  return cmd.str();
}

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
    for (auto& flag : conf::dialect_flags(*config.toolchain))
    {
      out.emplace_back(std::move(flag));
    }
  }
  for (const auto& flag : config.compile_flags)
  {
    out.emplace_back(flag.flag);
  }
  for (const auto& def : config.definitions)
  {
    std::string definition = "-D" + std::string(def.name);
    if (!def.value.empty())
    {
      definition += "=" + std::string(def.value);
    }
    out.push_back(std::move(definition));
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
  std::filesystem::path                           source;
  std::filesystem::path                           object;
  std::shared_ptr<const std::vector<std::string>> flags;
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

  void set_total(std::size_t total_to_set) noexcept
  {
    total = total_to_set;
    width = static_cast<int>(std::max<std::size_t>(1, std::to_string(total_to_set).size()));
  }

  void step(std::string_view action, std::string_view name)
  {
    if (total == 0)
    {
      return;
    }
    const auto             count = done.fetch_add(1, std::memory_order_relaxed) + 1;
    const std::scoped_lock lock(print_mu);
    std::cout << '[' << std::setw(width) << count << '/' << total << "] " << action << ' ' << name
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
      const std::size_t index = next.fetch_add(1, std::memory_order_relaxed);
      if (index >= items.size())
      {
        return;
      }
      try
      {
        work(items[index]);
      }
      catch (const std::exception& e)
      {
        const std::scoped_lock lock(err_mu);
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
  for (auto& thread : threads)
  {
    thread.join();
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
                              unsigned jobs, ProcessRunner& runner, Logger& logger,
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
      const std::size_t index = next.fetch_add(1, std::memory_order_relaxed);
      if (index >= tasks.size())
      {
        return;
      }
      const auto&                     task       = tasks[index];
      const std::vector<std::string>& task_flags = *task.flags;
      std::vector<std::string>        args;
      args.reserve(task_flags.size() + 4);
      args.emplace_back("-c");
      for (const auto& flag : task_flags)
      {
        args.push_back(flag);
      }
      args.push_back(task.source.string());
      args.emplace_back("-o");
      args.push_back(task.object.string());
      {
        const std::scoped_lock lock(log_mu);
        logger.debug("compile: " + format_command_for_log(compiler, args));
      }
      if (runner.run(ProcessRunRequest{
              .command = compiler, .args = std::move(args), .working_dir = ""}) != 0)
      {
        const std::scoped_lock lock(err_mu);
        if (!aborted.exchange(true))
        {
          first_err = "compilation failed: " + task.source.string();
        }
        return;
      }
      if (progress != nullptr)
      {
        progress->step("compiling", task.source.string());
      }
    }
  };

  std::vector<std::thread> threads;
  threads.reserve(worker_count);
  for (unsigned i = 0; i < worker_count; ++i)
  {
    threads.emplace_back(worker);
  }
  for (auto& thread : threads)
  {
    thread.join();
  }

  if (aborted.load())
  {
    throw std::runtime_error(first_err);
  }
}

// Per-framework resolved compile/link flags, keyed by `TestFramework::name`.
// Populated once before tests are built and consulted by both the planner
// and the actual build loop so cache keys stay stable.
using ResolvedTestFrameworks = std::map<std::string, cppup::plugin::TestBuildFlags>;

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
  ProcessRunner*                  process_runner;
  Logger*                         logger;
  ProgressReporter*               progress;
  // Non-null only when tests are being built and `config.test_frameworks`
  // has been resolved successfully. Reads from build paths key off the
  // per-test `framework` field; tests with an empty framework ignore this.
  const ResolvedTestFrameworks* test_framework_flags = nullptr;
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
  std::span<const std::string> extra_link_flags    = {};
  std::span<const std::string> extra_include_paths = {};
  std::filesystem::path        output_dir          = {};
};

bld::BuildTarget make_library_target(const conf::Library& library, const BuildContext& ctx);

// One library that needs a rebuild. Carries everything needed to archive
// after the flat compile phase finishes: the object list (input to ar)
// plus the BuildTarget so we can refresh the cache entry.
struct LibArchivePlan
{
  std::string                        name;
  std::filesystem::path              output_path;
  std::vector<std::filesystem::path> objects;
  bld::BuildTarget                   target;
};

// Cache-check `library` and, if it needs a rebuild, emit (CompileTask*,
// LibArchivePlan) describing the work. Cached libraries get a single log
// line and no plan entry. Returns true if the library was cached.
bool plan_library_build(const conf::Library& library, const BuildContext& ctx,
                        std::vector<CompileTask>& out_tasks, std::vector<LibArchivePlan>& out_plans)
{
  auto target = make_library_target(library, ctx);

  if (!ctx.cache->needs_rebuild(target))
  {
    ctx.logger->info("cached library: " + library.name);
    return true;
  }

  auto shared_flags = std::make_shared<std::vector<std::string>>(target.compile_flags);

  LibArchivePlan plan{
      .name        = library.name,
      .output_path = target.output_path,
      .objects     = {},
      .target      = target,
  };
  plan.objects.reserve(target.source_files.size());
  for (const auto& src : target.source_files)
  {
    auto obj = ctx.paths->build_dir / (src.stem().string() + "_" + library.name + ".o");
    out_tasks.push_back(CompileTask{.source = src, .object = obj, .flags = shared_flags});
    plan.objects.push_back(obj);
  }
  out_plans.push_back(std::move(plan));
  return false;
}

// Run `ar rcs` over the plan's objects and refresh the cache entry. Called
// once per non-cached library after the flat compile phase finishes.
void archive_library(const LibArchivePlan& plan, const BuildContext& ctx)
{
  std::vector<std::string> ar_args;
  ar_args.reserve(plan.objects.size() + 2);
  ar_args.emplace_back("rcs");
  ar_args.push_back(plan.output_path.string());
  for (const auto& obj : plan.objects)
  {
    ar_args.push_back(obj.string());
  }
  ctx.logger->debug("archive: " + format_command_for_log("ar", ar_args));
  if (ctx.process_runner->run(
          ProcessRunRequest{.command = "ar", .args = std::move(ar_args), .working_dir = ""}) != 0)
  {
    throw std::runtime_error("archive failed: " + plan.name);
  }
  ctx.progress->step("archiving", plan.output_path.filename().string());

  auto deps = collect_dependencies(*ctx.cache, plan.target);
  ctx.cache->cache_build_result(plan.target, deps);
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
  for (const auto& flag : resolved.library_flags)
  {
    link_flags.emplace_back(flag);
  }
  for (const auto& flag : config.link_flags)
  {
    link_flags.emplace_back(flag.flag);
  }
  for (const auto& flag : extra_link_flags)
  {
    link_flags.emplace_back(flag);
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
  target.name = spec.name;
  target.type = spec.kind;
  const std::string_view exe_ext =
      config.toolchain ? conf::executable_extension(config.toolchain->name) : std::string_view{};
  target.output_path = target_dir / (spec.name + std::string{exe_ext});
  for (const auto& src : spec.sources)
  {
    target.source_files.push_back(paths.project_root / src);
  }
  append_common_flags(target.compile_flags, config, paths.project_root, ctx.options);
  for (const auto& inc : config.include_paths)
  {
    target.include_paths.push_back(paths.project_root / inc);
  }
  // Framework-supplied include paths (e.g. gtest's own `include/`) are
  // absolute and not project-relative. Pushed onto both the compile
  // flags (so the test's TU sees `<gtest/gtest.h>`) and the include
  // path list used by collect_dependencies for header scanning.
  for (const auto& inc : spec.extra_include_paths)
  {
    target.compile_flags.push_back("-I" + inc);
    target.include_paths.emplace_back(inc);
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

// Per-test compile/link additions sourced from the test's framework
// (if any) plus the test's own `link_flags`. Held in a struct so the
// vectors live long enough to back the ExecutableSpec's spans.
struct TestExecutableInputs
{
  std::vector<std::string> extra_link_flags;
  std::vector<std::string> extra_include_paths;
};

TestExecutableInputs make_test_executable_inputs(const conf::Test& test, const BuildContext& ctx)
{
  TestExecutableInputs inputs;
  if (ctx.test_framework_flags != nullptr && !test.framework.empty())
  {
    const auto it = ctx.test_framework_flags->find(test.framework);
    CPPUP_CHECK(it != ctx.test_framework_flags->end(),
                "test framework not resolved (should be caught by validation)");
    const auto& framework_flags = it->second;
    inputs.extra_include_paths  = framework_flags.include_paths;
    for (const auto& path : framework_flags.library_paths)
    {
      inputs.extra_link_flags.push_back("-L" + path);
    }
    for (const auto& lib : framework_flags.libraries)
    {
      inputs.extra_link_flags.push_back("-l" + lib);
    }
    for (const auto& flag : framework_flags.link_flags)
    {
      inputs.extra_link_flags.push_back(flag);
    }
  }
  for (const auto& flag : test.link_flags)
  {
    inputs.extra_link_flags.emplace_back(flag.flag);
  }
  return inputs;
}

// Resolve every TestFramework in the project: locate its synced package,
// invoke the matching test-framework plugin's `build_and_get_flags` to
// materialize the framework's runtime artifacts, and remember the
// resulting flags keyed by `TestFramework::name`. Called once before any
// tests are built. Returns an error mentioning the offending framework
// when the plugin is missing, the package isn't synced, or the build
// step fails.
std::expected<ResolvedTestFrameworks, std::string> resolve_test_frameworks(
    const conf::BuildConfiguration& config, const std::filesystem::path& cppup_dir,
    ProcessRunner& runner, Logger& logger)
{
  ResolvedTestFrameworks resolved;
  if (config.test_frameworks.empty())
  {
    return resolved;
  }

  auto& registry = cppup::plugin::global_test_framework_registry();
  for (const auto& framework : config.test_frameworks)
  {
    const auto* plugin = registry.find(framework.plugin);
    if (plugin == nullptr)
    {
      return std::unexpected("test framework '" + framework.name + "': plugin '" +
                             framework.plugin + "' not registered");
    }
    const auto package_root = framework.package.has_value()
                                  ? cppup_dir / "packages" / framework.package->name()
                                  : cppup_dir / "packages" / framework.name;
    if (!std::filesystem::exists(package_root) || std::filesystem::is_empty(package_root))
    {
      return std::unexpected(
          "test framework '" + framework.name + "': package '" +
          (framework.package.has_value() ? framework.package->name() : framework.name) +
          "' not synced at " + package_root.string() + " (run `cppup sync`)");
    }
    const auto cache_dir = cppup_dir / "test_frameworks" / framework.name;
    logger.info("building test framework: " + framework.name);
    auto flags = plugin->build_and_get_flags(package_root, cache_dir, runner);
    if (!flags)
    {
      return std::unexpected("test framework '" + framework.name + "': " + flags.error());
    }
    resolved.emplace(framework.name, std::move(*flags));
  }
  return resolved;
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
  auto shared_flags = std::make_shared<std::vector<std::string>>(target.compile_flags);
  for (const auto& src : target.source_files)
  {
    auto obj =
        ctx.paths->build_dir / (src.stem().string() + "_" + spec.name + "_" + spec.kind + ".o");
    bin_tasks.push_back(CompileTask{.source = src, .object = obj, .flags = shared_flags});
    bin_objects.push_back(obj);
  }
  compile_objects_parallel(compiler, bin_tasks, ctx.options.jobs, *ctx.process_runner, *ctx.logger,
                           ctx.progress);

  std::vector<std::string> link_args;
  link_args.reserve(bin_objects.size() + target.link_flags.size() + 2);
  for (const auto& obj : bin_objects)
  {
    link_args.push_back(obj.string());
  }
  for (const auto& flag : target.link_flags)
  {
    link_args.push_back(flag);
  }
  link_args.emplace_back("-o");
  link_args.push_back(target.output_path.string());

  ctx.logger->debug("link: " + format_command_for_log(compiler, link_args));
  if (ctx.process_runner->run(ProcessRunRequest{
          .command = compiler, .args = std::move(link_args), .working_dir = ""}) != 0)
  {
    throw std::runtime_error(spec.kind + " link failed: " + spec.name);
  }
  ctx.progress->step("linking", target.output_path.filename().string());

  auto deps = collect_dependencies(*ctx.cache, target);
  ctx.cache->cache_build_result(target, deps);
  return false;
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
      const auto inputs = make_test_executable_inputs(test, ctx);
      total += exec_contribution({
          .kind                 = "test",
          .name                 = test.name,
          .sources              = test.sources,
          .linked_library_names = test.libraries,
          .extra_link_flags     = inputs.extra_link_flags,
          .extra_include_paths  = inputs.extra_include_paths,
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
//
// When the on-disk header content does change we also wipe
// `.cppup/build/config/` because the configuration-compiler's freshness
// check only tracks build.cpp's mtime, not the header it includes — a
// stale DSO compiled against an older layout would otherwise corrupt
// the in-process BuildConfiguration read on the next load.
void materialize_configuration_header(const std::filesystem::path& cppup_dir)
{
  const auto header_dir  = cppup_dir / "include" / "cppup";
  const auto header_path = header_dir / "configuration.hpp";
  std::filesystem::create_directories(header_dir);

  if (std::filesystem::exists(header_path))
  {
    std::ifstream const ifs(header_path, std::ios::binary);
    std::stringstream   buf;
    buf << ifs.rdbuf();
    if (buf.str() == kConfigurationHeader)
    {
      return;
    }
  }
  std::ofstream out(header_path, std::ios::binary | std::ios::trunc);
  out.write(kConfigurationHeader.data(), static_cast<std::streamsize>(kConfigurationHeader.size()));

  // Header changed: invalidate the configuration-DSO cache so subprojects
  // recompile against the new struct layout.
  const auto      config_cache = cppup_dir / "build" / "config";
  std::error_code error_code;
  std::filesystem::remove_all(config_cache, error_code);
}

conf::BuildConfiguration load_build_configuration(const std::filesystem::path& project_root,
                                                  const std::filesystem::path& cppup_dir)
{
  conf::CompilerOptions compiler_opts{};
  compiler_opts.include_paths.push_back((cppup_dir / "include").string());
  compiler_opts.include_paths.push_back((project_root / "include").string());
  compiler_opts.include_paths.push_back((project_root / "src").string());
  compiler_opts.output_directory = (cppup_dir / "build" / "config").string();

  conf::ConfigurationCompiler compiler{compiler_opts};
  auto                        config_result = conf::load_with_subprojects(project_root, compiler);
  CPPUP_CHECK(config_result.has_value(), "configuration compilation failed");
  return *config_result;
}

// Selection helpers (read_persisted_selection, migrate_legacy_toolchain_file,
// resolve_selection, apply_selection) live in selection_resolver.{hpp,cpp}
// so `cppup compile-commands` can apply the same precedence chain as the
// build path without duplicating the logic.

std::string format_project_summary(const conf::BuildConfiguration& config)
{
  const auto plural = [](std::size_t n, std::string_view singular, std::string_view plur)
  { return std::format("{} {}", n, (n == 1) ? singular : plur); };
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
                    const auto inputs = make_test_executable_inputs(test, ctx);
                    const bool cached = build_executable(
                        {
                            .kind                 = "test",
                            .name                 = test.name,
                            .sources              = test.sources,
                            .linked_library_names = test.libraries,
                            .extra_link_flags     = inputs.extra_link_flags,
                            .extra_include_paths  = inputs.extra_include_paths,
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

std::string format_wall_time(long long milliseconds)
{
  if (milliseconds < 1000)
  {
    return std::to_string(milliseconds) + "ms";
  }
  const long long whole_sec = milliseconds / 1000;
  const long long tenths    = (milliseconds % 1000) / 100;
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

std::expected<int, std::string> executeBuild(const conf::BuildOptions& options,
                                             const CommandContext&     context) noexcept
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

    // `cppup build` never reaches over the network. If `cppup.lock` lists
    // packages that aren't materialized under `.cppup/packages/`, fail fast
    // with the names so the user runs `cppup sync` deliberately — package
    // sizes range from header-only libs to multi-GB SDKs, and a silent
    // download on every build is hostile UX once toolchains-as-packages
    // land. A missing lockfile is treated as "nothing locked" and skipped.
    auto missing = find_unmaterialized_packages(context.projectRoot);
    if (!missing)
    {
      return std::unexpected(missing.error());
    }
    if (!missing->empty())
    {
      std::string message = "Missing packages (not materialized in .cppup/packages/):";
      for (const auto& name : *missing)
      {
        message += "\n  - " + name;
      }
      message += "\n\nRun `cppup sync` to install them.";
      return std::unexpected(message);
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

    migrate_legacy_toolchain_file(context.projectRoot, logger);

    // Export the resolved selection into the environment BEFORE compiling
    // and loading build.cpp so its `when_toolchain` / `when_profile` blocks
    // fire correctly inside configure(). Without this, selection would
    // only take effect after configure() has already finished and the
    // when_* lambdas would never have run.
    const auto persisted = read_persisted_selection(context.projectRoot);
    const auto early     = resolve_early_selection(options, persisted);
    export_selection_env(early);
    logger.debug("active toolchain: " + early.toolchain);
    if (!early.profile.empty())
    {
      logger.debug("active profile: " + early.profile);
    }

    auto base_config = load_build_configuration(context.projectRoot, cppup_dir);

    const auto selection = resolve_selection(options, persisted, base_config);
    if (auto applied = apply_selection(base_config, selection); !applied)
    {
      return std::unexpected(applied.error());
    }
    const auto& config = base_config;

    logger.info(format_project_summary(config));

    logger.debug(
        "wrote " +
        conf::emit_compile_commands(config, paths.project_root, paths.build_dir, options).string());

    for (const auto& sub_project : config.subprojects)
    {
      auto sub_result = run_subproject_via_plugin(
          sub_project, paths.project_root / sub_project.path, cppup::plugin::global_registry(),
          *context.processRunner, logger);
      if (!sub_result)
      {
        return std::unexpected(sub_result.error());
      }
    }

    // Resolve test-framework packages → plugin → flags map BEFORE any
    // test target is planned, so the planner sees the same compile/link
    // inputs the real build will use. Only runs when tests are being
    // built; plain `cppup build` skips both the resolution and the
    // per-test injection.
    ResolvedTestFrameworks resolved_frameworks;
    if (conf::enabled(options.with_tests))
    {
      auto resolved = resolve_test_frameworks(config, cppup_dir, *context.processRunner, logger);
      if (!resolved)
      {
        return std::unexpected(resolved.error());
      }
      resolved_frameworks = std::move(*resolved);
    }

    ProgressReporter   progress;
    const BuildContext ctx{
        .config               = &config,
        .paths                = &paths,
        .cache                = cache.get(),
        .options              = options,
        .process_runner       = context.processRunner.get(),
        .logger               = &logger,
        .progress             = &progress,
        .test_framework_flags = &resolved_frameworks,
    };
    progress.set_total(count_planned_steps(ctx));

    BuildCounters counts;

    // Libraries: flat-queue build. Phase A plans every non-cached library
    // into a single CompileTask list, Phase B compiles all sources at once
    // (saturating cores even when one library has 16 fat TUs), Phase C
    // archives each library in parallel since static archives have no
    // inter-library ordering constraint.
    //
    // Between A and B we sort tasks by source size descending (LPT
    // scheduling). Without it, libraries late in config.libraries (e.g.
    // cppup_cli with its 16 fat commands TUs) land at the tail of the
    // queue and only start after the small tasks ahead have drained the
    // pool — turning a 22-way burst into a long ~2-way tail.
    {
      std::vector<CompileTask>    lib_tasks;
      std::vector<LibArchivePlan> archive_plans;
      for (const auto& library : config.libraries)
      {
        if (plan_library_build(library, ctx, lib_tasks, archive_plans))
        {
          ++counts.cached;
        }
      }
      counts.built += config.libraries.size();

      std::ranges::sort(lib_tasks,
                        [](const CompileTask& lhs, const CompileTask& rhs)
                        {
                          std::error_code error;
                          const auto      lhs_size = std::filesystem::file_size(lhs.source, error);
                          const auto      rhs_size = std::filesystem::file_size(rhs.source, error);
                          return lhs_size > rhs_size;
                        });

      compile_objects_parallel("g++", lib_tasks, ctx.options.jobs, *ctx.process_runner, logger,
                               &progress);
      run_in_parallel(archive_plans, ctx.options.jobs,
                      [&](const LibArchivePlan& plan) { archive_library(plan, ctx); });
    }

    // Binaries/tests are still outer-parallel with inner jobs=1 — each
    // typically has one source, so inner parallelism wouldn't help anyway.
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
