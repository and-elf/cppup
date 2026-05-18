#include "directory_package.hpp"

using namespace cppup::configuration;

namespace cppup::package::directory
{

DirectoryPackage::DirectoryPackage(PackageInfo info) : info_(std::move(info)) {}

std::expected<std::filesystem::path, std::string> DirectoryPackage::resolve_source() const
{
  if (!info_.source_directory.has_value())
  {
    return std::unexpected("Source directory not specified");
  }

  std::filesystem::path source_path(info_.source_directory.value());
  if (!std::filesystem::exists(source_path))
  {
    return std::unexpected("Source directory does not exist: " + source_path.string());
  }

  return source_path;
}

}  // namespace cppup::package::directory