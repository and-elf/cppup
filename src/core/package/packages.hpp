#pragma once

/**
 * Main header for the cppup package system
 *
 * This header provides access to all package types and utilities.
 * The package system is organized into separate modules for each source type:
 *
 * - git::GitPackage - For Git repositories
 * - directory::DirectoryPackage - For local directories
 * - archive::ArchivePackage - For TAR/ZIP archives
 * - http::HttpPackage - For HTTP downloads
 * - registry::RegistryPackage - For registry packages (future)
 *
 * Each package type implements the PackageType concept and can be used
 * independently or through the PackageFactory.
 */

// Core concepts and utilities
#include "package_concept.hpp"
#include "package_factory.hpp"

// Configuration types
#include "../configuration/types.hpp"

// Individual package types
#include "archive/archive_package.hpp"
#include "directory/directory_package.hpp"
#include "git/git_package.hpp"
#include "http/http_package.hpp"
#include "registry/registry_package.hpp"

namespace cppup::package
{

/**
 * Convenience function to create a package from PackageInfo
 */
constexpr cppup::configuration::Package make_package(cppup::configuration::PackageInfo info)
{
  using namespace cppup::configuration;

  switch (info.source_type)
  {
    case SourceType::GIT:
      return Package(git::GitPackage(std::move(info)));
    case SourceType::DIRECTORY:
      return Package(directory::DirectoryPackage(std::move(info)));
    case SourceType::TAR:
    case SourceType::ZIP:
      return Package(archive::ArchivePackage(std::move(info)));
    case SourceType::HTTP:
      return Package(http::HttpPackage(std::move(info)));
    case SourceType::REGISTRY:
      return Package(registry::RegistryPackage(std::move(info)));
    default:
      // This should never happen in a well-formed program
      return Package(registry::RegistryPackage(std::move(info)));
  }
}

/**
 * Convenience function to create a package with command executor
 */
inline cppup::configuration::Package make_package(cppup::configuration::PackageInfo info,
                                                  std::shared_ptr<CommandExecutor>  executor)
{
  auto package = PackageFactory::create_package(std::move(info));
  package.set_command_executor(std::move(executor));
  return package;
}

/**
 * Convenience function to create a package with command executor and cache
 */
inline cppup::configuration::Package make_package(cppup::configuration::PackageInfo      info,
                                                  std::shared_ptr<CommandExecutor>       executor,
                                                  std::shared_ptr<PackageCacheInterface> cache)
{
  auto package = PackageFactory::create_package(std::move(info));
  package.set_command_executor(std::move(executor));
  package.set_cache(std::move(cache));
  return package;
}

}  // namespace cppup::package

/**
 * Helper functions for creating packages with common configurations
 */
namespace cppup::configuration::package_helpers
{

/**
 * Create a package from a Git repository
 */
constexpr Package from_git(std::string name, std::string url,
                           std::optional<std::string> branch = std::nullopt)
{
  PackageInfo info(std::move(name));
  info.url         = std::move(url);
  info.source_type = SourceType::GIT;
  if (branch.has_value())
  {
    info.git_branch = std::move(branch);
  }

  return Package(cppup::package::git::GitPackage(std::move(info)));
}

/**
 * Create a package from a local directory
 */
constexpr Package from_directory(std::string name, std::string directory)
{
  PackageInfo info(std::move(name));
  info.source_directory = std::move(directory);
  info.source_type      = SourceType::DIRECTORY;

  return Package(cppup::package::directory::DirectoryPackage(std::move(info)));
}

/**
 * Create a package from a TAR archive
 */
constexpr Package from_tar(std::string name, std::string url)
{
  PackageInfo info(std::move(name));
  info.url         = std::move(url);
  info.source_type = SourceType::TAR;

  return Package(cppup::package::archive::ArchivePackage(std::move(info)));
}

/**
 * Create a package from a ZIP archive
 */
constexpr Package from_zip(std::string name, std::string url)
{
  PackageInfo info(std::move(name));
  info.url         = std::move(url);
  info.source_type = SourceType::ZIP;

  return Package(cppup::package::archive::ArchivePackage(std::move(info)));
}

/**
 * Create a header-only package (typically from Git)
 */
constexpr Package header_only(std::string name, std::string url)
{
  PackageInfo info(std::move(name));
  info.url         = std::move(url);
  info.source_type = SourceType::GIT;
  // Note: Header-only detection will be handled by the build system

  return Package(cppup::package::git::GitPackage(std::move(info)));
}

/**
 * Create a package from HTTP download
 */
constexpr Package from_http(std::string name, std::string url)
{
  PackageInfo info(std::move(name));
  info.url         = std::move(url);
  info.source_type = SourceType::HTTP;

  return Package(cppup::package::http::HttpPackage(std::move(info)));
}

/**
 * Create a package from a registry (default source type)
 */
constexpr Package from_registry(std::string name, std::optional<std::string> version = std::nullopt)
{
  PackageInfo info(std::move(name));
  if (version.has_value())
  {
    info.version = std::move(version);
  }
  info.source_type = SourceType::REGISTRY;

  return Package(cppup::package::registry::RegistryPackage(std::move(info)));
}

/**
 * Create a package with specific Git commit
 */
constexpr Package from_git_commit(std::string name, std::string url, std::string commit)
{
  PackageInfo info(std::move(name));
  info.url         = std::move(url);
  info.source_type = SourceType::GIT;
  info.git_commit  = std::move(commit);

  return Package(cppup::package::git::GitPackage(std::move(info)));
}

/**
 * Create a package with specific Git branch and commit
 */
constexpr Package from_git_branch_commit(std::string name, std::string url, std::string branch,
                                         std::string commit)
{
  PackageInfo info(std::move(name));
  info.url         = std::move(url);
  info.source_type = SourceType::GIT;
  info.git_branch  = std::move(branch);
  info.git_commit  = std::move(commit);

  return Package(cppup::package::git::GitPackage(std::move(info)));
}

}  // namespace cppup::configuration::package_helpers