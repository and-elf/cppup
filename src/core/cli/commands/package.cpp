#include <algorithm>
#include <chrono>
#include <concepts>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <logger.hpp>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>

// Package system includes
#include "../../configuration/types.hpp"
#include "../../package/packages.hpp"

// Simplified types for bootstrap build
namespace cppup::cli
{

// Concept for package installers
template <typename T>
concept PackageInstaller =
    requires(T installer, Logger* logger, const cppup::configuration::Package& package,
             const std::filesystem::path& install_path) {
      { installer.installPackage(logger, package, install_path) } -> std::same_as<bool>;
    };

// Bootstrap package installer - minimal implementation
class BootstrapPackageInstaller
{
 public:
  bool installPackage(Logger* logger, const cppup::configuration::Package& package,
                      const std::filesystem::path& install_path)
  {
    // Resolve the package source
    auto source_result = package.resolve_source();
    if (!source_result)
    {
      logger->error("Failed to resolve package source: " + source_result.error());
      return false;
    }

    auto        source_path = source_result.value();
    std::string name        = package.name();

    // Bootstrap build only supports basic git packages
    using cppup::configuration::SourceType;
    if (package.info().source_type == SourceType::GIT)
    {
      return installGitPackage(logger, package, name, install_path);
    }
    logger->error("Bootstrap build only supports git packages");
    return false;
  }

 private:
  bool installGitPackage(Logger* logger, const cppup::configuration::Package& package,
                         const std::string& name, const std::filesystem::path& install_path)
  {
    logger->info("Installing git package: " + name + " from " + package.info().url.value_or(""));

    // For bootstrap, we still need to clone since resolve_source might not be fully implemented
    // In a full implementation, this would just copy from the resolved source
    std::string source_url = package.info().url.value_or("");
    if (source_url.empty())
    {
      logger->error("No URL specified for git package: " + name);
      return false;
    }

    // Create install directory
    std::filesystem::create_directories(install_path);

    // Clone the repository
    std::string clone_cmd = "git clone " + source_url + " " + install_path.string();
    if (std::system(clone_cmd.c_str()) != 0)
    {
      logger->error("Failed to clone repository: " + source_url);
      return false;
    }

    logger->info("Successfully installed package: " + name);
    return true;
  }
};

// Full package installer - complete implementation
class FullPackageInstaller
{
 public:
  bool installPackage(Logger* logger, const cppup::configuration::Package& package,
                      const std::filesystem::path& install_path)
  {
    // Resolve the package source
    auto source_result = package.resolve_source();
    if (!source_result)
    {
      logger->error("Failed to resolve package source: " + source_result.error());
      return false;
    }

    auto        source_path = source_result.value();
    std::string name        = package.name();

    // Use package source type for cleaner installation logic
    using cppup::configuration::SourceType;

    switch (package.info().source_type)
    {
      case SourceType::GIT:
        return installGitPackage(logger, source_path, name, install_path);
      case SourceType::DIRECTORY:
        return installDirectoryPackage(logger, source_path, name, install_path);
      case SourceType::TAR:
      case SourceType::ZIP:
        return installArchivePackage(logger, source_path, name, install_path);
      case SourceType::HTTP:
        return installHttpPackage(logger, source_path, name, install_path);
      case SourceType::REGISTRY:
        return installRegistryPackage(logger, source_path, name, install_path);
      default:
        logger->error("Unsupported source type for package: " + name);
        return false;
    }
  }

 private:
  bool installGitPackage(Logger* logger, const std::filesystem::path& source_path,
                         const std::string& name, const std::filesystem::path& install_path)
  {
    logger->info("Installing git package: " + name + " from " + source_path.string());

    // Copy from resolved source to install path
    try
    {
      std::filesystem::create_directories(install_path);
      std::filesystem::copy(source_path, install_path, std::filesystem::copy_options::recursive);
      logger->info("Successfully installed git package: " + name);
      return true;
    }
    catch (const std::filesystem::filesystem_error& e)
    {
      logger->error("Failed to copy git package: " + std::string(e.what()));
      return false;
    }
  }

  bool installDirectoryPackage(Logger* logger, const std::filesystem::path& source_path,
                               const std::string& name, const std::filesystem::path& install_path)
  {
    logger->info("Installing directory package: " + name + " from " + source_path.string());

    try
    {
      std::filesystem::create_directories(install_path);
      std::filesystem::copy(source_path, install_path, std::filesystem::copy_options::recursive);
      logger->info("Successfully installed directory package: " + name);
      return true;
    }
    catch (const std::filesystem::filesystem_error& e)
    {
      logger->error("Failed to copy directory package: " + std::string(e.what()));
      return false;
    }
  }

  bool installArchivePackage(Logger* logger, const std::filesystem::path& source_path,
                             const std::string& name, const std::filesystem::path& install_path)
  {
    logger->info("Installing archive package: " + name + " from " + source_path.string());

    try
    {
      std::filesystem::create_directories(install_path);
      std::filesystem::copy(source_path, install_path, std::filesystem::copy_options::recursive);
      logger->info("Successfully installed archive package: " + name);
      return true;
    }
    catch (const std::filesystem::filesystem_error& e)
    {
      logger->error("Failed to copy archive package: " + std::string(e.what()));
      return false;
    }
  }

  bool installHttpPackage(Logger* logger, const std::filesystem::path& source_path,
                          const std::string& name, const std::filesystem::path& install_path)
  {
    logger->info("Installing HTTP package: " + name + " from " + source_path.string());

    try
    {
      std::filesystem::create_directories(install_path);
      std::filesystem::copy(source_path, install_path, std::filesystem::copy_options::recursive);
      logger->info("Successfully installed HTTP package: " + name);
      return true;
    }
    catch (const std::filesystem::filesystem_error& e)
    {
      logger->error("Failed to copy HTTP package: " + std::string(e.what()));
      return false;
    }
  }

  bool installRegistryPackage(Logger* logger, const std::filesystem::path& source_path,
                              const std::string& name, const std::filesystem::path& install_path)
  {
    logger->info("Installing registry package: " + name + " from " + source_path.string());

    // For registry packages, the source might be a git repo or other
    // For now, just copy like other packages
    try
    {
      std::filesystem::create_directories(install_path);
      std::filesystem::copy(source_path, install_path, std::filesystem::copy_options::recursive);
      logger->info("Successfully installed registry package: " + name);
      return true;
    }
    catch (const std::filesystem::filesystem_error& e)
    {
      logger->error("Failed to copy registry package: " + std::string(e.what()));
      return false;
    }
  }
};

/**
 * Command context containing all dependencies needed by commands
 */
struct CommandContext
{
  std::filesystem::path   project_root;
  std::unique_ptr<Logger> logger;

  // Default constructor
  CommandContext() = default;

  // Move constructor and assignment
  CommandContext(CommandContext&&)            = default;
  CommandContext& operator=(CommandContext&&) = default;

  // Delete copy operations
  CommandContext(const CommandContext&)            = delete;
  CommandContext& operator=(const CommandContext&) = delete;
};

struct PackageAddOptions
{
  std::string                name;
  std::optional<std::string> version;
  std::optional<std::string> tag;
  std::optional<std::string> url;
  std::optional<std::string> dir;

  // Git-specific options
  std::optional<std::string> git;
  std::optional<std::string> branch;
  std::optional<std::string> commit;

  // Build system options
  bool header_only = false;
  bool cmake       = false;
  bool make        = false;
  bool meson       = false;
  bool autotools   = false;

  // Additional options
  std::optional<std::string> build_args;
  std::optional<std::string> subdirectory;
};

}  // namespace cppup::cli

namespace cppup::cli
{

/**
 * Simple package database for bootstrap version
 */
struct BootstrapPackageInfo
{
  std::string name;
  std::string version;
  std::string description;
  std::string install_path;
  int64_t     install_time = 0;
};

/**
 * Bootstrap package manager - simplified for bootstrap build
 */
class BootstrapPackageManager
{
 public:
  explicit BootstrapPackageManager(const std::filesystem::path& workspace_root) :
      workspace_root_(workspace_root), packages_dir_(workspace_root / ".cppup" / "packages")
  {
  }

  bool initialize()
  {
    try
    {
      std::filesystem::create_directories(packages_dir_);
      load_packages();
      return true;
    }
    catch (const std::exception&)
    {
      return false;
    }
  }

  std::vector<BootstrapPackageInfo> list_installed() const
  {
    return installed_packages_;
  }

  bool install_package(const std::string& name, const std::string& version = "")
  {
    BootstrapPackageInfo pkg;
    pkg.name         = name;
    pkg.version      = version.empty() ? "latest" : version;
    pkg.description  = "Package installed via cppup";
    pkg.install_path = (packages_dir_ / name).string();
    pkg.install_time = std::chrono::duration_cast<std::chrono::seconds>(
                           std::chrono::system_clock::now().time_since_epoch())
                           .count();

    installed_packages_.push_back(pkg);
    save_packages();
    return true;
  }

  bool remove_package(const std::string& name)
  {
    auto it = std::find_if(installed_packages_.begin(), installed_packages_.end(),
                           [&](const BootstrapPackageInfo& pkg) { return pkg.name == name; });

    if (it == installed_packages_.end())
    {
      return false;
    }

    installed_packages_.erase(it);
    save_packages();
    return true;
  }

 private:
  std::filesystem::path             workspace_root_;
  std::filesystem::path             packages_dir_;
  std::vector<BootstrapPackageInfo> installed_packages_;

  void load_packages()
  {
    auto packages_file = packages_dir_ / "installed.txt";
    if (!std::filesystem::exists(packages_file))
    {
      return;
    }

    std::ifstream file(packages_file);
    std::string   line;
    while (std::getline(file, line))
    {
      std::istringstream   iss(line);
      BootstrapPackageInfo pkg;
      if (iss >> pkg.name >> pkg.version >> pkg.install_time)
      {
        std::getline(iss, pkg.description);  // Get rest of line
        if (!pkg.description.empty())
        {
          pkg.description = pkg.description.substr(1);  // Remove leading space
        }
        pkg.install_path = (packages_dir_ / pkg.name).string();
        installed_packages_.push_back(pkg);
      }
    }
  }

  void save_packages()
  {
    auto          packages_file = packages_dir_ / "installed.txt";
    std::ofstream file(packages_file);

    for (const auto& pkg : installed_packages_)
    {
      file << pkg.name << " " << pkg.version << " " << pkg.install_time << " " << pkg.description
           << std::endl;
    }
  }
};

bool buildPackage(const PackageAddOptions& options, const CommandContext& context,
                  const std::filesystem::path& package_dir)
{
  if (!std::filesystem::exists(package_dir))
  {
    context.logger->error("Package directory not found: " + package_dir.string());
    return false;
  }

  context.logger->info("Building package '" + options.name + "' in: " + package_dir.string());

  // Change to package directory
  std::filesystem::current_path(package_dir);

  // Determine build system and build
  bool build_success = false;

  if (options.cmake)
  {
    context.logger->info("Building with CMake...");
    std::string cmake_cmd = "mkdir -p build && cd build && cmake ..";
    if (options.build_args)
    {
      cmake_cmd += " " + options.build_args.value();
    }
    cmake_cmd += " && make -j$(nproc)";

    int cmake_result = std::system(cmake_cmd.c_str());
    build_success    = (cmake_result == 0);
  }
  else if (options.make)
  {
    context.logger->info("Building with Make...");
    std::string make_cmd = "make -j$(nproc)";
    if (options.build_args)
    {
      make_cmd += " " + options.build_args.value();
    }

    int make_result = std::system(make_cmd.c_str());
    build_success   = (make_result == 0);
  }
  else if (options.meson)
  {
    context.logger->info("Building with Meson...");
    std::string meson_cmd = "mkdir -p build && meson build && cd build && ninja";
    if (options.build_args)
    {
      // Apply build args to meson setup
      meson_cmd =
          "mkdir -p build && meson build " + options.build_args.value() + " && cd build && ninja";
    }

    int meson_result = std::system(meson_cmd.c_str());
    build_success    = (meson_result == 0);
  }
  else if (options.autotools)
  {
    context.logger->info("Building with Autotools...");
    std::string autotools_cmd = "./configure";
    if (options.build_args)
    {
      autotools_cmd += " " + options.build_args.value();
    }
    autotools_cmd += " && make -j$(nproc)";

    int autotools_result = std::system(autotools_cmd.c_str());
    build_success        = (autotools_result == 0);
  }
  else if (options.header_only)
  {
    context.logger->info("Header-only package - no build required");
    build_success = true;
  }
  else
  {
    // Auto-detect build system
    context.logger->info("Auto-detecting build system...");

    if (std::filesystem::exists("CMakeLists.txt"))
    {
      context.logger->info("Detected CMake project");
      std::string cmake_cmd = "mkdir -p build && cd build && cmake .. && make -j$(nproc)";
      int         result    = std::system(cmake_cmd.c_str());
      build_success         = (result == 0);
    }
    else if (std::filesystem::exists("Makefile"))
    {
      context.logger->info("Detected Make project");
      std::string make_cmd = "make -j$(nproc)";
      int         result   = std::system(make_cmd.c_str());
      build_success        = (result == 0);
    }
    else if (std::filesystem::exists("configure"))
    {
      context.logger->info("Detected Autotools project");
      std::string configure_cmd = "./configure && make -j$(nproc)";
      int         result        = std::system(configure_cmd.c_str());
      build_success             = (result == 0);
    }
    else
    {
      context.logger->info("No build system detected - assuming header-only or pre-built");
      build_success = true;
    }
  }

  return build_success;
}

int executePackageList(const CommandContext& context)
{
  try
  {
    BootstrapPackageManager pkg_manager(context.project_root);

    if (!pkg_manager.initialize())
    {
      context.logger->info("Failed to initialize package manager");
      return 1;
    }

    const auto& packages = pkg_manager.list_installed();

    if (packages.empty())
    {
      context.logger->info("No packages installed");
      context.logger->info("Use 'cppup package add <package_name>' to install packages");
      return 0;
    }

    // Display packages in a nice table format
    context.logger->info("Installed packages:");
    std::cout << std::left << std::setw(30) << "Package" << std::setw(15) << "Version"
              << "Description" << std::endl;
    std::cout << std::string(80, '-') << std::endl;

    for (const auto& package : packages)
    {
      std::cout << std::left << std::setw(30) << package.name << std::setw(15) << package.version;

      // Truncate description if too long
      std::string desc = package.description;
      if (desc.length() > 35)
      {
        desc = desc.substr(0, 32) + "...";
      }
      std::cout << desc << std::endl;

      // Show install time if available
      if (package.install_time > 0)
      {
        auto install_time =
            std::chrono::system_clock::time_point(std::chrono::seconds(package.install_time));
        auto time_t = std::chrono::system_clock::to_time_t(install_time);
        std::cout << "  Installed: " << std::ctime(&time_t);
      }

      std::cout << std::endl;
    }

    context.logger->info(std::to_string(packages.size()) + " packages installed");
    return 0;
  }
  catch (const std::exception& e)
  {
    context.logger->info("Package list failed: " + std::string(e.what()));
    return 1;
  }
}

int executePackageAdd(const PackageAddOptions& options, const CommandContext& context)
{
  try
  {
    context.logger->info("Installing package: " + options.name);

    BootstrapPackageManager pkg_manager(context.project_root);

    if (!pkg_manager.initialize())
    {
      context.logger->info("Failed to initialize package manager");
      return 1;
    }

    // Create package using from_* helper functions or fallback for bootstrap
    cppup::configuration::Package package = [&]()
    {
#ifndef IS_BOOTSTRAP_BUILD
      using namespace cppup::configuration::package_helpers;

      if (options.git)
      {
        // Git package
        if (options.branch)
        {
          return from_git(options.name, options.git.value(), options.branch.value());
        }
        else
        {
          return from_git(options.name, options.git.value());
        }
      }
      else if (options.url)
      {
        // URL package - determine type from URL
        std::string url = options.url.value();
        if (url.find(".tar.gz") != std::string::npos || url.find(".tgz") != std::string::npos)
        {
          return from_tar(options.name, url);
        }
        else if (url.find(".zip") != std::string::npos)
        {
          return from_zip(options.name, url);
        }
        else
        {
          return from_http(options.name, url);
        }
      }
      else if (options.dir)
      {
        // Local directory package
        return from_directory(options.name, options.dir.value());
      }
      else
      {
        // Default to registry
        if (options.version)
        {
          return from_registry(options.name, options.version.value());
        }
        else
        {
          return from_registry(options.name);
        }
      }
#else
      // Bootstrap fallback - create a dummy package that will be handled by the installer
      // This is a temporary solution until the full package system is integrated
      cppup::configuration::PackageInfo info(options.name);
      if (options.git)
      {
        info.url         = options.git.value();
        info.source_type = cppup::configuration::SourceType::GIT;
        if (options.branch)
        {
          info.git_branch = options.branch.value();
        }
      }
      else if (options.url)
      {
        info.url         = options.url.value();
        info.source_type = cppup::configuration::SourceType::HTTP;
      }
      else if (options.dir)
      {
        info.source_directory = options.dir.value();
        info.source_type      = cppup::configuration::SourceType::DIRECTORY;
      }
      else
      {
        info.source_type = cppup::configuration::SourceType::REGISTRY;
      }

      // For bootstrap, we'll use a simple registry package as placeholder
      return cppup::configuration::Package(
          cppup::package::registry::RegistryPackage(std::move(info)));
#endif
    }();

    // Determine install path
    std::filesystem::path packages_dir = context.project_root / ".cppup" / "packages";
    std::filesystem::path install_path = packages_dir / options.name;
    std::filesystem::create_directories(packages_dir);

    // Use concept-based installer selection
#ifdef IS_BOOTSTRAP_BUILD
    BootstrapPackageInstaller installer;
#else
    FullPackageInstaller installer;
#endif

    // Install the package
    if (!installer.installPackage(context.logger.get(), package, install_path))
    {
      context.logger->error("Failed to install package: " + options.name);
      return 1;
    }

    // Handle version constraints and additional options
    std::string version = options.version.value_or("latest");
    if (options.tag)
    {
      version = options.tag.value();
      context.logger->info("Using tag/version: " + version);
    }

    // Register package
    if (!pkg_manager.install_package(options.name, version))
    {
      context.logger->error("Failed to register package in package manager");
      return 1;
    }

    context.logger->info("Package '" + options.name + "' installed successfully");
    return 0;
  }
  catch (const std::exception& e)
  {
    context.logger->error("Package installation failed: " + std::string(e.what()));
    return 1;
  }
}

int executePackageRemove(const std::string& package_name, const CommandContext& context)
{
  try
  {
    context.logger->info("Removing package: " + package_name);

    BootstrapPackageManager pkg_manager(context.project_root);

    if (!pkg_manager.initialize())
    {
      context.logger->info("Failed to initialize package manager");
      return 1;
    }

    if (!pkg_manager.remove_package(package_name))
    {
      context.logger->info("Package '" + package_name + "' not found");
      return 1;
    }

    context.logger->info("Package '" + package_name + "' removed successfully");
    context.logger->info(
        "Note: This is a bootstrap version - full cleanup requires complete cppup build");
    return 0;
  }
  catch (const std::exception& e)
  {
    context.logger->info("Package removal failed: " + std::string(e.what()));
    return 1;
  }
}

}  // namespace cppup::cli
