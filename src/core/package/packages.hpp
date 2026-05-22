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
#include "../panic.hpp"
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
      [[fallthrough]];
    case SourceType::ZIP:
      return Package(archive::ArchivePackage(std::move(info)));
    case SourceType::HTTP:
      return Package(http::HttpPackage(std::move(info)));
    case SourceType::REGISTRY:
      return Package(registry::RegistryPackage(std::move(info)));
  }
  ::cppup::panic("make_package: unhandled SourceType");
}

/**
 * Convenience function to create a package with command executor
 */
inline cppup::configuration::Package make_package(cppup::configuration::PackageInfo       info,
                                                  const std::shared_ptr<CommandExecutor>& executor)
{
  auto package = PackageFactory::create_package(std::move(info));
  package.set_command_executor(executor);
  return package;
}

/**
 * Convenience function to create a package with command executor and cache
 */
inline cppup::configuration::Package make_package(
    cppup::configuration::PackageInfo info, const std::shared_ptr<CommandExecutor>& executor,
    const std::shared_ptr<PackageCacheInterface>& cache)
{
  auto package = PackageFactory::create_package(std::move(info));
  package.set_command_executor(executor);
  package.set_cache(cache);
  return package;
}

}  // namespace cppup::package
