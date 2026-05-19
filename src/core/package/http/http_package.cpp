#include "http_package.hpp"

using namespace cppup::configuration;

namespace cppup::package::http
{

HttpPackage::HttpPackage(PackageInfo info) : info_(std::move(info)) {}

std::expected<std::filesystem::path, std::string> HttpPackage::resolve_source() const
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
    return std::unexpected("HTTP URL not specified");
  }

  auto cache_path = cache_->get_package_cache_path(info_.name, info_);

  // Check if already cached
  if (cache_->is_cached(info_.name, info_))
  {
    return cache_path;
  }

  return download_resource();
}

std::expected<std::filesystem::path, std::string> HttpPackage::download_resource() const
{
  auto cache_path = cache_->get_package_cache_path(info_.name, info_);

  std::filesystem::create_directories(cache_path.parent_path());

  // Determine file extension from URL
  std::string const           url = info_.url.value();
  std::filesystem::path const url_path(url);
  std::string const           extension = url_path.extension().string();

  auto download_path = cache_path.parent_path() / (info_.name + extension);

  if (!utils::download_file(*command_executor_, url, download_path))
  {
    return std::unexpected("Failed to download HTTP resource");
  }

  // If it's an archive, extract it
  if (is_archive_extension(extension))
  {
    if (!utils::extract_archive(*command_executor_, download_path, cache_path))
    {
      return std::unexpected("Failed to extract downloaded archive");
    }
    std::filesystem::remove(download_path);
    return cache_path;
  }
  else
  {
    // Single file - move to cache directory
    std::filesystem::create_directories(cache_path);
    auto final_path = cache_path / url_path.filename();
    std::filesystem::rename(download_path, final_path);
    return cache_path;
  }
}

bool HttpPackage::is_archive_extension(const std::string& extension) const
{
  return extension == ".tar" || extension == ".gz" || extension == ".tgz" || extension == ".zip" ||
         extension == ".7z";
}

}  // namespace cppup::package::http