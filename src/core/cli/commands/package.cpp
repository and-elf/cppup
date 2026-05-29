#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <expected>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "../../configuration/compiler.hpp"
#include "../../configuration/subproject.hpp"
#include "../../configuration/subproject_loader.hpp"
#include "command_context.hpp"
#include "commands.hpp"
#include "embedded_configuration_header.hpp"
#include "install_paths.hpp"
#include "lockfile.hpp"

namespace cppup::cli
{

namespace
{

struct PackageRecord
{
  std::string name;
  std::string version;
  std::string source;
  // "cppup" | "cmake" | "make" | "header-only". Empty for legacy entries
  // written before this field existed.
  std::string build_system;
  int64_t     installed_at = 0;
};

std::string build_system_name(cppup::configuration::BuildSystem buildsystem) noexcept
{
  using cppup::configuration::BuildSystem;
  switch (buildsystem)
  {
    case BuildSystem::Cppup:
      return "cppup";
    case BuildSystem::CMake:
      return "cmake";
    case BuildSystem::Make:
      return "make";
    case BuildSystem::HeaderOnly:
      return "header-only";
  }
  return "";
}

class PackageRegistry
{
 public:
  // `data_root` is the `.cppup`-equivalent directory (project or user scope);
  // the registry creates / reads `<data_root>/packages/` and the manifest at
  // `<data_root>/packages/registry.txt`.
  explicit PackageRegistry(const std::filesystem::path& data_root) :
      packages_dir_(data_root / "packages"), registry_file_(packages_dir_ / "registry.txt")
  {
  }

  // Convenience: build a project-scoped registry from a project root.
  [[nodiscard]] static PackageRegistry for_project(const std::filesystem::path& project_root)
  {
    return PackageRegistry{project_data_dir(project_root)};
  }

  [[nodiscard]] bool ensure_directories() noexcept
  {
    try
    {
      std::filesystem::create_directories(packages_dir_);
      return true;
    }
    catch (const std::exception&)
    {
      return false;
    }
  }

  [[nodiscard]] std::vector<PackageRecord> load() const
  {
    std::vector<PackageRecord> records;
    if (!std::filesystem::exists(registry_file_))
    {
      return records;
    }
    std::ifstream ifs(registry_file_);
    std::string   line;
    while (std::getline(ifs, line))
    {
      if (line.empty())
      {
        continue;
      }
      std::istringstream iss(line);
      PackageRecord      rec{};
      std::string        installed_at_str;
      // Tab-separated: name <TAB> version <TAB> source <TAB> installed_at
      // [<TAB> build_system]. The trailing build_system field is optional so
      // we keep reading entries written before it existed.
      if (std::getline(iss, rec.name, '\t') && std::getline(iss, rec.version, '\t') &&
          std::getline(iss, rec.source, '\t') && std::getline(iss, installed_at_str, '\t'))
      {
        try
        {
          rec.installed_at = std::stoll(installed_at_str);
        }
        catch (...)
        {
          rec.installed_at = 0;
        }
        std::getline(iss, rec.build_system);
        records.push_back(std::move(rec));
      }
    }
    return records;
  }

  [[nodiscard]] bool save(const std::vector<PackageRecord>& records) const
  {
    std::ofstream registry_output(registry_file_);
    if (!registry_output)
    {
      return false;
    }
    for (const auto& rec : records)
    {
      registry_output << rec.name << '\t' << rec.version << '\t' << rec.source << '\t'
                      << rec.installed_at << '\t' << rec.build_system << '\n';
    }
    return true;
  }

  [[nodiscard]] const std::filesystem::path& packages_dir() const noexcept
  {
    return packages_dir_;
  }

 private:
  std::filesystem::path packages_dir_;
  std::filesystem::path registry_file_;
};

int64_t now_epoch()
{
  return std::chrono::duration_cast<std::chrono::seconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}

std::string describePackageSource(const PackageAddOptions& options)
{
  if (options.git)
  {
    return "git:" + *options.git;
  }
  if (options.url)
  {
    return "url:" + *options.url;
  }
  if (options.dir)
  {
    return "dir:" + *options.dir;
  }
  return "registry";
}

bool fetchGitPackage(GitInterface& git, const std::string& url, const std::filesystem::path& dest,
                     const std::optional<std::string>& branch,
                     GitVerbosity                      verbosity = GitVerbosity::Quiet)
{
  return git.clone_shallow(url, dest, branch, verbosity);
}

bool copyLocalPackage(const std::filesystem::path& src, const std::filesystem::path& dest)
{
  try
  {
    std::filesystem::create_directories(dest);
    std::filesystem::copy(src, dest,
                          std::filesystem::copy_options::recursive |
                              std::filesystem::copy_options::overwrite_existing);
    return true;
  }
  catch (const std::exception&)
  {
    return false;
  }
}

std::expected<cppup::configuration::BuildConfiguration, std::string> load_project_configuration(
    const std::filesystem::path& project_root)
{
  if (!std::filesystem::exists(project_root / "build.cpp"))
  {
    return std::unexpected("No build.cpp found in: " + project_root.string());
  }
  const auto cppup_dir = project_root / ".cppup";

  // The configuration compiler needs the embedded `configuration.hpp` to be
  // present on disk so build.cpp can #include it. `cppup build` does this
  // via `materialize_configuration_header`; we repeat the relevant bits
  // here so `package lock` works on a fresh checkout that hasn't been built.
  const auto      header_dir  = cppup_dir / "include" / "cppup";
  const auto      header_path = header_dir / "configuration.hpp";
  std::error_code error_code;
  std::filesystem::create_directories(header_dir, error_code);
  bool need_write = true;
  if (std::filesystem::exists(header_path))
  {
    const std::ifstream ifs(header_path, std::ios::binary);
    std::stringstream   buf;
    buf << ifs.rdbuf();
    need_write = buf.str() != kConfigurationHeader;
  }
  if (need_write)
  {
    std::ofstream out(header_path, std::ios::binary | std::ios::trunc);
    out.write(kConfigurationHeader.data(),
              static_cast<std::streamsize>(kConfigurationHeader.size()));
  }

  cppup::configuration::CompilerOptions compiler_opts{};
  compiler_opts.include_paths.push_back((cppup_dir / "include").string());
  compiler_opts.include_paths.push_back((project_root / "include").string());
  compiler_opts.include_paths.push_back((project_root / "src").string());
  compiler_opts.output_directory = (cppup_dir / "build" / "config").string();

  cppup::configuration::ConfigurationCompiler compiler{compiler_opts};
  auto config = cppup::configuration::load_with_subprojects(project_root, compiler);
  if (!config)
  {
    return std::unexpected("Failed to load build.cpp: " + config.error());
  }
  return *config;
}

[[nodiscard]] std::filesystem::path lockfile_path(
    const std::filesystem::path& project_root) noexcept
{
  return project_root / "cppup.lock";
}

// Materialize one lockfile entry into `.cppup/packages/<name>/`. Returns
// true if the on-disk state is now valid for the entry; false if the fetch
// failed. Idempotent: if the destination already has content we leave it
// alone and report success.
bool materialize_entry(const lockfile::Entry& entry, const std::filesystem::path& install_path,
                       const CommandContext& context, GitVerbosity verbosity = GitVerbosity::Quiet)
{
  if (std::filesystem::exists(install_path) && !std::filesystem::is_empty(install_path))
  {
    return true;
  }
  switch (entry.source)
  {
    case lockfile::SourceKind::Git:
    {
      if (context.git == nullptr)
      {
        context.logger->warning("git interface not configured; cannot sync " + entry.name);
        return false;
      }
      const std::optional<std::string> branch =
          entry.git_branch.empty() ? std::nullopt : std::optional{entry.git_branch};
      return fetchGitPackage(*context.git, entry.url, install_path, branch, verbosity);
    }
    case lockfile::SourceKind::Directory:
    {
      if (entry.url.empty())
      {
        context.logger->warning("directory source missing path; cannot sync " + entry.name);
        return false;
      }
      return copyLocalPackage(entry.url, install_path);
    }
    case lockfile::SourceKind::Url:
    case lockfile::SourceKind::Tar:
    case lockfile::SourceKind::Zip:
    case lockfile::SourceKind::Registry:
    {
      // Match `package add`'s behaviour for these sources: create an empty
      // placeholder so the registry stays consistent. Real fetch support
      // for archives lands separately.
      std::error_code error_code;
      std::filesystem::create_directories(install_path, error_code);
      return !error_code;
    }
  }
  return false;
}

}  // namespace

std::expected<int, std::string> executePackageList(const CommandContext& context) noexcept
{
  try
  {
    struct ScopedRecord
    {
      PackageRecord rec;
      const char*   scope_tag;
    };

    std::vector<ScopedRecord> all;
    const auto add_from = [&](const std::filesystem::path& data_root, const char* scope_tag)
    {
      const PackageRegistry registry(data_root);
      if (!std::filesystem::exists(registry.packages_dir()))
      {
        return;
      }
      for (auto& rec : registry.load())
      {
        all.push_back({std::move(rec), scope_tag});
      }
    };

    // Always show the project-scoped registry, even when its directory is
    // absent — the legacy contract `ensure_directories()` honoured.
    PackageRegistry project_registry(project_data_dir(context.projectRoot));
    if (!project_registry.ensure_directories())
    {
      return std::unexpected("Could not initialize package directory");
    }
    for (auto& rec : project_registry.load())
    {
      all.push_back({std::move(rec), "project"});
    }
    if (auto user_root = user_data_dir())
    {
      add_from(*user_root, "user");
    }

    if (all.empty())
    {
      context.logger->info("No packages installed");
      context.logger->info("Add packages with: cppup package add --name <name>");
      return 0;
    }

    context.logger->info("Installed packages (" + std::to_string(all.size()) + "):");
    for (const auto& entry : all)
    {
      std::string line = "  " + entry.rec.name + " " + entry.rec.version + " [" + entry.rec.source +
                         "] (" + entry.scope_tag + ")";
      if (!entry.rec.build_system.empty())
      {
        line += " {" + entry.rec.build_system + "}";
      }
      context.logger->info(line);
    }
    return 0;
  }
  catch (const std::exception& e)
  {
    return std::unexpected("Package list failed: " + std::string(e.what()));
  }
}

std::expected<int, std::string> executePackageAdd(const PackageAddOptions& options,
                                                  const CommandContext&    context) noexcept
{
  try
  {
    if (options.name.empty())
    {
      return std::unexpected("Package name is required (--name)");
    }
    if (options.version && options.tag)
    {
      return std::unexpected("--version and --tag are mutually exclusive");
    }
    if (options.url && options.dir)
    {
      return std::unexpected("--url and --dir are mutually exclusive");
    }

    const auto install_root = resolve_install_root(options.scope, context.projectRoot);
    if (!install_root)
    {
      return std::unexpected("Cannot resolve user data directory: set HOME or XDG_DATA_HOME");
    }

    PackageRegistry registry(*install_root);
    if (!registry.ensure_directories())
    {
      return std::unexpected("Could not initialize package directory");
    }

    auto records = registry.load();
    if (std::ranges::any_of(
            records, [&](const PackageRecord& rec) noexcept { return rec.name == options.name; }))
    {
      return std::unexpected("Package already installed: " + options.name);
    }

    const std::filesystem::path install_path = registry.packages_dir() / options.name;
    context.logger->info(std::string{"Installing package ("} +
                         (is_user(options.scope) ? "user" : "project") + "): " + options.name);

    bool fetched = false;
    if (options.git)
    {
      if (context.git == nullptr)
      {
        return std::unexpected("No git interface configured");
      }
      context.logger->info("Cloning from: " + *options.git);
      fetched = fetchGitPackage(*context.git, *options.git, install_path, options.branch);
    }
    else if (options.dir)
    {
      context.logger->info("Copying from local directory: " + *options.dir);
      fetched = copyLocalPackage(*options.dir, install_path);
    }
    else if (options.url)
    {
      context.logger->warning("URL fetch not supported in this build; recording reference only");
      std::filesystem::create_directories(install_path);
      fetched = true;
    }
    else
    {
      context.logger->info("No source specified; registering as registry package placeholder");
      std::filesystem::create_directories(install_path);
      fetched = true;
    }

    if (!fetched)
    {
      return std::unexpected("Failed to fetch package: " + options.name);
    }

    // Decide the build system. Explicit `--build-system` wins; otherwise probe
    // the fetched directory (rooted at `--subdirectory` if given). Probing can
    // fail if nothing was actually fetched (e.g. the --url stub or a registry
    // placeholder), so we treat inference failure as a soft warning.
    const auto package_root =
        options.subdirectory ? install_path / *options.subdirectory : install_path;
    std::string build_system_value;
    if (options.build_system)
    {
      build_system_value = *options.build_system;
    }
    else
    {
      auto inferred = cppup::configuration::infer_build_system(package_root);
      if (inferred)
      {
        build_system_value = build_system_name(*inferred);
      }
      else
      {
        context.logger->warning("Could not infer build system for " + options.name + ": " +
                                inferred.error());
      }
    }
    if (!build_system_value.empty())
    {
      context.logger->info("Build system: " + build_system_value);
    }

    PackageRecord record;
    record.name         = options.name;
    record.version      = options.tag.value_or(options.version.value_or("latest"));
    record.source       = describePackageSource(options);
    record.build_system = build_system_value;
    record.installed_at = now_epoch();
    records.push_back(std::move(record));

    if (!registry.save(records))
    {
      return std::unexpected("Failed to update package registry");
    }

    context.logger->info("Package installed: " + options.name);
    return 0;
  }
  catch (const std::exception& e)
  {
    return std::unexpected("Package add failed: " + std::string(e.what()));
  }
}

std::expected<int, std::string> executePackageRemove(const std::string&    package_name,
                                                     const CommandContext& context) noexcept
{
  try
  {
    if (package_name.empty())
    {
      return std::unexpected("Package name is required");
    }

    // Search project then user; remove from the first registry that owns the
    // name. Project wins on name collisions (matches the search order).
    std::vector<std::filesystem::path> roots;
    roots.push_back(project_data_dir(context.projectRoot));
    if (auto user_root = user_data_dir())
    {
      roots.push_back(std::move(*user_root));
    }

    for (const auto& root : roots)
    {
      const PackageRegistry registry(root);
      if (!std::filesystem::exists(registry.packages_dir()))
      {
        continue;
      }
      auto       records = registry.load();
      const auto iter    = std::ranges::find_if(
          records, [&](const PackageRecord& rec) noexcept { return rec.name == package_name; });
      if (iter == records.end())
      {
        continue;
      }

      const std::filesystem::path install_path = registry.packages_dir() / package_name;
      if (std::filesystem::exists(install_path))
      {
        std::error_code error_code{};
        std::filesystem::remove_all(install_path, error_code);
        if (error_code)
        {
          context.logger->warning("Could not remove files at " + install_path.string() + ": " +
                                  error_code.message());
        }
      }

      records.erase(iter);
      if (!registry.save(records))
      {
        return std::unexpected("Failed to update package registry");
      }

      context.logger->info("Package removed: " + package_name);
      return 0;
    }

    return std::unexpected("Package not found: " + package_name);
  }
  catch (const std::exception& e)
  {
    return std::unexpected("Package remove failed: " + std::string(e.what()));
  }
}

std::expected<int, std::string> executePackageLock(const CommandContext& context) noexcept
{
  try
  {
    auto config = load_project_configuration(context.projectRoot);
    if (!config)
    {
      return std::unexpected(config.error());
    }

    auto entries = lockfile::entries_from_configuration(*config);
    if (!entries)
    {
      return std::unexpected(entries.error());
    }
    const auto serialized = lockfile::serialize(*entries);

    const auto    path = lockfile_path(context.projectRoot);
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out)
    {
      return std::unexpected("Failed to open " + path.string() + " for writing");
    }
    out.write(serialized.data(), static_cast<std::streamsize>(serialized.size()));
    if (!out)
    {
      return std::unexpected("Failed to write " + path.string());
    }

    context.logger->info("Wrote " + path.filename().string() + " (" +
                         std::to_string(entries->size()) + " packages)");
    return 0;
  }
  catch (const std::exception& e)
  {
    return std::unexpected("Package lock failed: " + std::string(e.what()));
  }
}

std::expected<int, std::string> executePackageSync(const PackageSyncOptions& options,
                                                   const CommandContext&     context) noexcept
{
  try
  {
    const auto path = lockfile_path(context.projectRoot);
    if (!std::filesystem::exists(path))
    {
      return std::unexpected("No cppup.lock at project root. Run `cppup package lock` first.");
    }
    const std::ifstream ifs(path, std::ios::binary);
    std::stringstream   buf;
    buf << ifs.rdbuf();
    auto parsed = lockfile::parse(buf.str());
    if (!parsed)
    {
      return std::unexpected("Failed to parse cppup.lock: " + parsed.error());
    }

    PackageRegistry registry(project_data_dir(context.projectRoot));
    if (!registry.ensure_directories())
    {
      return std::unexpected("Could not initialize package directory");
    }

    auto       records = registry.load();
    const auto lookup  = [&](const std::string& name)
    {
      return std::ranges::find_if(
          records, [&](const PackageRecord& rec) noexcept { return rec.name == name; });
    };

    const GitVerbosity git_verbosity =
        options.verbose == Verbose::On ? GitVerbosity::Verbose : GitVerbosity::Quiet;

    // Phase 1: classify every lockfile entry from a snapshot of on-disk state.
    // We do this serially so the user-visible "Syncing package: <name>" lines
    // come out in lockfile order, and so the fetch worker pool sees a stable
    // job list.
    struct EntryPlan
    {
      std::filesystem::path install_path;
      bool                  needs_fetch{false};
      bool                  needs_metadata_reconcile{false};
      bool                  had_metadata_at_start{false};
    };
    std::vector<EntryPlan>   plans(parsed->size());
    std::vector<std::size_t> fetch_indices;
    std::size_t              unchanged{};

    for (std::size_t i = 0; i < parsed->size(); ++i)
    {
      const auto& entry      = (*parsed)[i];
      auto&       plan       = plans[i];
      plan.install_path      = registry.packages_dir() / entry.name;
      const bool dir_present = std::filesystem::exists(plan.install_path) &&
                               !std::filesystem::is_empty(plan.install_path);
      plan.had_metadata_at_start = (lookup(entry.name) != records.end());

      if (dir_present && plan.had_metadata_at_start)
      {
        ++unchanged;
        continue;
      }
      plan.needs_metadata_reconcile = true;
      if (!dir_present)
      {
        plan.needs_fetch = true;
        fetch_indices.push_back(i);
        context.logger->info("Syncing package: " + entry.name);
      }
    }

    // Phase 2: fan fetches out across a worker pool. Cap by the user's
    // --jobs setting (0 = hardware_concurrency), then by the actual job
    // count, then by 1 as a floor for hosts that report 0 cores.
    std::vector<std::optional<std::string>> errors_by_index(parsed->size());
    if (!fetch_indices.empty())
    {
      unsigned hw = std::thread::hardware_concurrency();
      if (hw == 0)
      {
        hw = 4;
      }
      unsigned cap = options.jobs > 0 ? options.jobs : hw;
      cap          = std::min<unsigned>(cap, static_cast<unsigned>(fetch_indices.size()));
      if (cap == 0)
      {
        cap = 1;
      }

      std::atomic<std::size_t> next{0};
      const auto               worker = [&]
      {
        for (;;)
        {
          const auto slot = next.fetch_add(1, std::memory_order_relaxed);
          if (slot >= fetch_indices.size())
          {
            return;
          }
          const auto  idx   = fetch_indices[slot];
          const auto& entry = (*parsed)[idx];
          if (!materialize_entry(entry, plans[idx].install_path, context, git_verbosity))
          {
            errors_by_index[idx] = "Failed to fetch package: " + entry.name;
          }
        }
      };

      if (cap == 1)
      {
        worker();
      }
      else
      {
        std::vector<std::jthread> workers;
        workers.reserve(cap);
        for (unsigned t = 0; t < cap; ++t)
        {
          workers.emplace_back(worker);
        }
        // jthread destructors join here.
      }

      // Surface the first failure in lockfile order so error messages stay
      // reproducible across runs regardless of worker scheduling.
      for (const auto& maybe_err : errors_by_index)
      {
        if (maybe_err)
        {
          return std::unexpected(*maybe_err);
        }
      }
    }

    // Phase 3: reconcile registry metadata in lockfile order. Single-threaded
    // because `records` is a plain vector and `lookup` walks it linearly.
    std::size_t fetched = fetch_indices.size();
    std::size_t repaired_metadata{};
    for (std::size_t i = 0; i < parsed->size(); ++i)
    {
      if (!plans[i].needs_metadata_reconcile)
      {
        continue;
      }
      const auto& entry = (*parsed)[i];
      // We treat the lockfile as truth: name/version/source/build_system come
      // from there, and installed_at is reset on repair so users can see
      // when the local copy was last touched.
      PackageRecord record;
      record.name    = entry.name;
      record.version = entry.version.empty() ? "latest" : entry.version;
      record.source  = std::string(lockfile::to_string(entry.source));
      if (!entry.url.empty())
      {
        record.source += ":" + entry.url;
      }
      record.build_system = entry.build_system;
      record.installed_at = now_epoch();

      if (plans[i].had_metadata_at_start)
      {
        if (auto it = lookup(entry.name); it != records.end())
        {
          *it = std::move(record);
          ++repaired_metadata;
          continue;
        }
      }
      records.push_back(std::move(record));
    }

    if (!registry.save(records))
    {
      return std::unexpected("Failed to update package registry");
    }

    context.logger->info("Sync complete: " + std::to_string(fetched) + " fetched, " +
                         std::to_string(repaired_metadata) + " metadata repaired, " +
                         std::to_string(unchanged) + " unchanged");
    return 0;
  }
  catch (const std::exception& e)
  {
    return std::unexpected("Package sync failed: " + std::string(e.what()));
  }
}

std::expected<std::vector<std::string>, std::string> find_unmaterialized_packages(
    const std::filesystem::path& project_root)
{
  const auto path = lockfile_path(project_root);
  if (!std::filesystem::exists(path))
  {
    return std::vector<std::string>{};
  }
  const std::ifstream ifs(path, std::ios::binary);
  std::stringstream   buf;
  buf << ifs.rdbuf();
  auto parsed = lockfile::parse(buf.str());
  if (!parsed)
  {
    return std::unexpected("Failed to parse cppup.lock: " + parsed.error());
  }
  const auto packages_dir = project_data_dir(project_root) / "packages";

  std::vector<std::string> missing;
  for (const auto& entry : *parsed)
  {
    const auto      install_path = packages_dir / entry.name;
    std::error_code error_code;
    const bool      dir_present = std::filesystem::exists(install_path, error_code) &&
                             !std::filesystem::is_empty(install_path, error_code);
    if (!dir_present)
    {
      missing.push_back(entry.name);
    }
  }
  return missing;
}

}  // namespace cppup::cli
