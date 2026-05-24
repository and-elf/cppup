#include "header_only_package.hpp"

#include <filesystem>

#include "../../package/packages.hpp"

using cppup::configuration::Package;
using cppup::configuration::PackageInfo;

namespace cppup::buildsystems::header_only
{

HeaderOnlyPackage::HeaderOnlyPackage(PackageInfo info) : info_(std::move(info)) {}

void HeaderOnlyPackage::ensure_source_package() const
{
  if (!source_package_)
  {
    source_package_ = std::make_unique<Package>(cppup::package::make_package(info_));
    if (command_executor_)
    {
      source_package_->set_command_executor(command_executor_);
    }
  }
}

std::expected<std::filesystem::path, std::string> HeaderOnlyPackage::resolve_source() const
{
  ensure_source_package();
  if (!source_package_)
  {
    return std::unexpected("Failed to create source package");
  }
  return source_package_->resolve_source();
}

std::expected<void, std::string> HeaderOnlyPackage::build(
    const std::filesystem::path& source_path) const
{
  const auto actual_source_path = cppup::package::utils::get_actual_source_path(source_path, info_);

  auto found = find_header_directories(actual_source_path);
  if (found.empty())
  {
    found.push_back(actual_source_path.string());
  }
  include_paths_ = std::move(found);

  // Header-only libraries don't have a real build step.
  return {};
}

std::vector<std::string> HeaderOnlyPackage::find_header_directories(
    const std::filesystem::path& source_path)
{
  std::vector<std::string> result;

  const std::vector<std::string> candidates = {"include", "src", "headers", "single_include", "."};

  for (const auto& dir : candidates)
  {
    const auto include_path = source_path / dir;
    if (!std::filesystem::exists(include_path) || !std::filesystem::is_directory(include_path))
    {
      continue;
    }

    bool has_headers = false;
    try
    {
      for (const auto& entry : std::filesystem::recursive_directory_iterator(include_path))
      {
        if (!entry.is_regular_file())
        {
          continue;
        }
        const auto extension = entry.path().extension().string();
        if (extension == ".h" || extension == ".hpp" || extension == ".hxx" ||
            extension == ".h++" || extension == ".hh")
        {
          has_headers = true;
          break;
        }
      }
    }
    catch (const std::filesystem::filesystem_error&)
    {
      // Skip directories we can't read.
      continue;
    }

    if (has_headers)
    {
      result.push_back(include_path.string());
    }
  }

  return result;
}

}  // namespace cppup::buildsystems::header_only
