#include "package_factory.hpp"

using namespace cppup::configuration;

namespace cppup::package
{

Package PackageFactory::create_package(PackageInfo info)
{
  auto creators = get_package_creators();
  auto it       = creators.find(info.source_type);

  if (it == creators.end())
  {
    throw std::runtime_error("Unsupported source type: " +
                             std::to_string(static_cast<int>(info.source_type)));
  }

  return it->second(std::move(info));
}

bool PackageFactory::is_source_type_supported(SourceType source_type)
{
  auto creators = get_package_creators();
  return creators.contains(source_type);
}

std::vector<SourceType> PackageFactory::get_supported_source_types()
{
  auto                    creators = get_package_creators();
  std::vector<SourceType> types;
  types.reserve(creators.size());

  for (const auto& [type, _] : creators)
  {
    types.push_back(type);
  }

  return types;
}

std::map<SourceType, PackageFactory::PackageCreator> PackageFactory::get_package_creators()
{
  static std::map<SourceType, PackageCreator> const creators = {
      {SourceType::GIT,
       [](PackageInfo info) -> Package { return Package(git::GitPackage(std::move(info))); }},
      {SourceType::DIRECTORY, [](PackageInfo info) -> Package
       { return Package(directory::DirectoryPackage(std::move(info))); }},
      {SourceType::TAR, [](PackageInfo info) -> Package
       { return Package(archive::ArchivePackage(std::move(info))); }},
      {SourceType::ZIP, [](PackageInfo info) -> Package
       { return Package(archive::ArchivePackage(std::move(info))); }},
      {SourceType::HTTP,
       [](PackageInfo info) -> Package { return Package(http::HttpPackage(std::move(info))); }},
      {SourceType::REGISTRY, [](PackageInfo info) -> Package
       { return Package(registry::RegistryPackage(std::move(info))); }}};

  return creators;
}

}  // namespace cppup::package