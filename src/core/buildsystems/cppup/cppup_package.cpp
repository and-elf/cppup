#include "cppup_package.hpp"

#include <filesystem>

#include "../../package/packages.hpp"

using namespace cppup::configuration;

namespace cppup::buildsystems::cppup_system
{

CppupPackage::CppupPackage(PackageInfo info) : info_(std::move(info)) {}

std::expected<std::filesystem::path, std::string> CppupPackage::resolve_source() const
{
  ensure_source_package();

  if (!source_package_)
  {
    return std::unexpected("Failed to create source package");
  }

  return source_package_->resolve_source();
}

void CppupPackage::ensure_source_package() const
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

std::expected<void, std::string> CppupPackage::build(const std::filesystem::path& source_path) const
{
  auto actual_source_path = cppup::package::utils::get_actual_source_path(source_path, info_);

  // Check for build.cpp
  if (!has_build_file(actual_source_path))
  {
    return std::unexpected("No build.cpp found in source directory: " +
                           actual_source_path.string());
  }

  // Execute cppup build
  auto build_result = execute_cppup_build(actual_source_path);
  if (!build_result)
  {
    return build_result;
  }

  // Setup build flags based on build output
  auto build_path = actual_source_path / "build";
  if (std::filesystem::exists(build_path))
  {
    setup_build_flags(actual_source_path, build_path);
  }

  return {};
}

bool CppupPackage::has_build_file(const std::filesystem::path& source_path)
{
  return std::filesystem::exists(source_path / "build.cpp");
}

std::expected<void, std::string> CppupPackage::execute_cppup_build(
    const std::filesystem::path& source_path) const
{
  if (!command_executor_)
  {
    return std::unexpected("No command executor available");
  }

  std::string build_command = "cppup build";

  // Add build arguments
  for (const auto& arg : info_.build_args)
  {
    build_command += " " + arg;
  }

  auto result =
      cppup::package::utils::execute_command(*command_executor_, build_command, source_path);
  if (!result)
  {
    return std::unexpected("cppup build failed: " + result.error());
  }

  return {};
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
void CppupPackage::setup_build_flags(const std::filesystem::path& source_path,
                                     const std::filesystem::path& build_path) const
{
  // Set up include paths
  include_paths_.clear();

  // Common include directories
  if (std::filesystem::exists(source_path / "include"))
  {
    include_paths_.push_back((source_path / "include").string());
  }
  if (std::filesystem::exists(source_path / "src"))
  {
    include_paths_.push_back((source_path / "src").string());
  }

  // Set up library paths
  library_paths_.clear();
  library_paths_.push_back(build_path.string());

  // Set up libraries (scan build directory for .a/.so/.dll files)
  link_flags_.clear();
  for (const auto& entry : std::filesystem::recursive_directory_iterator(build_path))
  {
    if (entry.is_regular_file())
    {
      auto extension = entry.path().extension().string();
      if (extension == ".a" || extension == ".so" || extension == ".dll" || extension == ".lib")
      {
        auto lib_name = entry.path().stem().string();
        // Remove lib prefix if present
        if (lib_name.starts_with("lib"))
        {
          lib_name = lib_name.substr(3);
        }
        link_flags_.push_back("-l" + lib_name);
      }
    }
  }
}

}  // namespace cppup::buildsystems::cppup_system