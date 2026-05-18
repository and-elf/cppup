#include "header_only_package.hpp"

#include <filesystem>

namespace cppup::buildsystems::header_only
{

HeaderOnlyPackage::HeaderOnlyPackage(cppup::configuration::PackageInfo info) :
    PackageBase(std::move(info))
{
}

std::expected<std::filesystem::path, std::string> HeaderOnlyPackage::resolve_source() const
{
  switch (info().source_type)
  {
    case SourceType::GIT:
      return resolve_git_source();
    case SourceType::DIRECTORY:
      return resolve_directory_source();
    case SourceType::TAR:
    case SourceType::ZIP:
      return resolve_archive_source();
    case SourceType::HTTP:
      return resolve_http_source();
    case SourceType::REGISTRY:
      return std::unexpected("Registry packages not supported by header-only build system");
    default:
      return std::unexpected("Unknown source type");
  }
}

std::expected<void, std::string> HeaderOnlyPackage::build(
    const std::filesystem::path& source_path) const
{
  auto actual_source_path = get_actual_source_path(source_path);

  // Header-only libraries don't need building, just setup include paths
  setup_include_paths(actual_source_path);

  return {};
}

void HeaderOnlyPackage::setup_include_paths(const std::filesystem::path& source_path) const
{
  auto include_paths = find_header_directories(source_path);

  // If no specific include directories found, use the source path itself
  if (include_paths.empty())
  {
    include_paths.push_back(source_path.string());
  }

  const_cast<HeaderOnlyPackage*>(this)->set_include_paths(std::move(include_paths));
}

std::vector<std::string> HeaderOnlyPackage::find_header_directories(
    const std::filesystem::path& source_path) const
{
  std::vector<std::string> include_paths;

  // Common header directory names
  std::vector<std::string> possible_dirs = {"include", "src", "headers", "single_include", "."};

  for (const auto& dir : possible_dirs)
  {
    auto include_path = source_path / dir;
    if (std::filesystem::exists(include_path) && std::filesystem::is_directory(include_path))
    {
      // Check if directory contains header files
      bool has_headers = false;

      try
      {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(include_path))
        {
          if (entry.is_regular_file())
          {
            auto extension = entry.path().extension().string();
            if (extension == ".h" || extension == ".hpp" || extension == ".hxx" ||
                extension == ".h++" || extension == ".hh")
            {
              has_headers = true;
              break;
            }
          }
        }
      }
      catch (const std::filesystem::filesystem_error&)
      {
        // Skip directories we can't read
        continue;
      }

      if (has_headers)
      {
        include_paths.push_back(include_path.string());
      }
    }
  }

  return include_paths;
}

}  // namespace cppup::buildsystems::header_only

// Register the package type
REGISTER_PACKAGE_TYPE(cppup::buildsystems::header_only::HeaderOnlyPackage, "header_only");