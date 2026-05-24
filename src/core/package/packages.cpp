#include "packages.hpp"

#include "../panic.hpp"
#include "archive/archive_package.hpp"
#include "directory/directory_package.hpp"
#include "git/git_package.hpp"
#include "http/http_package.hpp"
#include "registry/registry_package.hpp"

namespace cppup::package
{

cppup::configuration::Package make_package(cppup::configuration::PackageInfo info)
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

}  // namespace cppup::package
