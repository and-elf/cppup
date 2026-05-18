#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <expected>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

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
  int64_t     installed_at = 0;
};

class PackageRegistry
{
 public:
  explicit PackageRegistry(const std::filesystem::path& project_root) :
      packages_dir_(project_root / ".cppup" / "packages"),
      registry_file_(packages_dir_ / "registry.txt")
  {
  }

  bool ensure_directories() noexcept
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

  std::vector<PackageRecord> load() const
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
      if (std::getline(iss, r.name, '\t') && std::getline(iss, r.version, '\t') &&
          std::getline(iss, r.source, '\t') && std::getline(iss, installed_at_str))
      {
        try
        {
          r.installed_at = std::stoll(installed_at_str);
        }
        catch (...)
        {
          r.installed_at = 0;
        }
        records.push_back(std::move(r));
      }
    }
    return records;
  }

  bool save(const std::vector<PackageRecord>& records) const
  {
    std::ofstream f(registry_file_);
    if (!f)
    {
      return false;
    }
    for (const auto& r : records)
    {
      f << r.name << '\t' << r.version << '\t' << r.source << '\t' << r.installed_at << '\n';
    }
    return true;
  }

  const std::filesystem::path& packages_dir() const noexcept
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
      context.logger->info("  " + r.name + " " + r.version + " [" + r.source + "]");
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
    if (std::any_of(records.begin(), records.end(),
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

    PackageRecord record;
    record.name         = options.name;
    record.version      = options.tag.value_or(options.version.value_or("latest"));
    record.source       = describePackageSource(options);
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
    const auto it      = std::find_if(records.begin(), records.end(),
                                      [&](const PackageRecord& r) { return r.name == package_name; });
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
