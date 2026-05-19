#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <expected>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "../../configuration/subproject.hpp"
#include "command_context.hpp"
#include "commands.hpp"

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

std::string build_system_name(cppup::configuration::BuildSystem bs) noexcept
{
  using cppup::configuration::BuildSystem;
  switch (bs)
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
  explicit PackageRegistry(const std::filesystem::path& project_root) :
      packages_dir_(project_root / ".cppup" / "packages"),
      registry_file_(packages_dir_ / "registry.txt")
  {
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
    std::ifstream f(registry_file_);
    std::string   line;
    while (std::getline(f, line))
    {
      if (line.empty())
      {
        continue;
      }
      std::istringstream iss(line);
      PackageRecord      r;
      std::string        installed_at_str;
      // Tab-separated: name <TAB> version <TAB> source <TAB> installed_at
      // [<TAB> build_system]. The trailing build_system field is optional so
      // we keep reading entries written before it existed.
      if (std::getline(iss, r.name, '\t') && std::getline(iss, r.version, '\t') &&
          std::getline(iss, r.source, '\t') && std::getline(iss, installed_at_str, '\t'))
      {
        try
        {
          r.installed_at = std::stoll(installed_at_str);
        }
        catch (...)
        {
          r.installed_at = 0;
        }
        std::getline(iss, r.build_system);
        records.push_back(std::move(r));
      }
    }
    return records;
  }

  [[nodiscard]] bool save(const std::vector<PackageRecord>& records) const
  {
    std::ofstream f(registry_file_);
    if (!f)
    {
      return false;
    }
    for (const auto& r : records)
    {
      f << r.name << '\t' << r.version << '\t' << r.source << '\t' << r.installed_at << '\t'
        << r.build_system << '\n';
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

bool fetchGitPackage(const std::string& url, const std::filesystem::path& dest,
                     const std::optional<std::string>& branch)
{
  std::string cmd = "git clone --depth 1";
  if (branch)
  {
    cmd += " --branch \"" + *branch + "\"";
  }
  cmd += " \"" + url + "\" \"" + dest.string() + "\"";
  return std::system(cmd.c_str()) == 0;
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

}  // namespace

std::expected<int, std::string> executePackageList(const CommandContext& context) noexcept
{
  try
  {
    PackageRegistry registry(context.projectRoot);
    if (!registry.ensure_directories())
    {
      return std::unexpected("Could not initialize package directory");
    }

    const auto records = registry.load();
    if (records.empty())
    {
      context.logger->info("No packages installed");
      context.logger->info("Add packages with: cppup package add --name <name>");
      return 0;
    }

    context.logger->info("Installed packages (" + std::to_string(records.size()) + "):");
    for (const auto& r : records)
    {
      std::string line = "  " + r.name + " " + r.version + " [" + r.source + "]";
      if (!r.build_system.empty())
      {
        line += " (" + r.build_system + ")";
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

    PackageRegistry registry(context.projectRoot);
    if (!registry.ensure_directories())
    {
      return std::unexpected("Could not initialize package directory");
    }

    auto records = registry.load();
    if (std::ranges::any_of(records,
                            [&](const PackageRecord& r) { return r.name == options.name; }))
    {
      return std::unexpected("Package already installed: " + options.name);
    }

    const std::filesystem::path install_path = registry.packages_dir() / options.name;
    context.logger->info("Installing package: " + options.name);

    bool fetched = false;
    if (options.git)
    {
      context.logger->info("Cloning from: " + *options.git);
      fetched = fetchGitPackage(*options.git, install_path, options.branch);
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

    PackageRegistry registry(context.projectRoot);
    if (!registry.ensure_directories())
    {
      return std::unexpected("Could not initialize package directory");
    }

    auto       records = registry.load();
    const auto it      = std::ranges::find_if(
        records, [&](const PackageRecord& r) { return r.name == package_name; });
    if (it == records.end())
    {
      return std::unexpected("Package not found: " + package_name);
    }

    const std::filesystem::path install_path = registry.packages_dir() / package_name;
    if (std::filesystem::exists(install_path))
    {
      std::error_code ec;
      std::filesystem::remove_all(install_path, ec);
      if (ec)
      {
        context.logger->warning("Could not remove files at " + install_path.string() + ": " +
                                ec.message());
      }
    }

    records.erase(it);
    if (!registry.save(records))
    {
      return std::unexpected("Failed to update package registry");
    }

    context.logger->info("Package removed: " + package_name);
    return 0;
  }
  catch (const std::exception& e)
  {
    return std::unexpected("Package remove failed: " + std::string(e.what()));
  }
}

}  // namespace cppup::cli
