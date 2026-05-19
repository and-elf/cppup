#include "archive_package.hpp"

using namespace cppup::configuration;

namespace cppup::package::archive
{

ArchivePackage::ArchivePackage(PackageInfo info) : info_(std::move(info)) {}

std::expected<std::filesystem::path, std::string> ArchivePackage::resolve_source() const
{
  if (!command_executor_)
  {
    return std::unexpected("No command executor available");
  }

  if (!cache_)
  {
    return std::unexpected("No cache interface available");
  }

  if (!info_.url.has_value())
  {
    return std::unexpected("Archive URL not specified");
  }

  auto cache_path = cache_->get_package_cache_path(info_.name, info_);

  // Check if already cached
  if (cache_->is_cached(info_.name, info_))
  {
    return cache_path;
  }

  return download_and_extract();
}

std::expected<std::filesystem::path, std::string> ArchivePackage::download_and_extract() const
{
  auto cache_path = cache_->get_package_cache_path(info_.name, info_);

  // Download archive
  std::string const extension    = get_archive_extension();
  auto              archive_path = cache_path.parent_path() / (info_.name + extension);

  std::filesystem::create_directories(cache_path.parent_path());

  if (!utils::download_file(*command_executor_, info_.url.value(), archive_path))
  {
    return std::unexpected("Failed to download archive");
  }

  // Extract archive
  if (!utils::extract_archive(*command_executor_, archive_path, cache_path))
  {
    return std::unexpected("Failed to extract archive");
  }

  // Clean up archive file
  std::filesystem::remove(archive_path);

  return cache_path;
}

std::string ArchivePackage::get_archive_extension() const
{
  switch (info_.source_type)
  {
    case SourceType::TAR:
      return ".tar.gz";
    case SourceType::ZIP:
      return ".zip";
    default:
      return ".tar.gz";  // Default fallback
  }
}

}  // namespace cppup::package::archive