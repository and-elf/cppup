#include "registry_package.hpp"

using namespace cppup::configuration;

namespace cppup::package::registry
{

RegistryPackage::RegistryPackage(PackageInfo info) : info_(std::move(info)) {}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static) -- signature matches PackageType
// concept
std::expected<std::filesystem::path, std::string> RegistryPackage::resolve_source() const
{
  // Registry packages are not yet implemented.
  return std::unexpected("Registry packages are not yet supported");
}

}  // namespace cppup::package::registry