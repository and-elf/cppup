#include "make_package.hpp"

#include <filesystem>
#include <sstream>

namespace cppup::buildsystems::make
{

MakePackage::MakePackage(cppup::configuration::PackageInfo info) : PackageBase(std::move(info)) {}

std::expected<std::filesystem::path, std::string> MakePackage::resolve_source() const
{
  switch (info().source_type)
  {
    case configuration::SourceType::GIT:
      return resolve_source();
    case configuration::SourceType::DIRECTORY:
      return resolve_directory_source();
    case configuration::SourceType::TAR:
    case configuration::SourceType::ZIP:
      return resolve_archive_source();
    case configuration::SourceType::HTTP:
      return resolve_source();
    case configuration::SourceType::REGISTRY:
      return std::unexpected("Registry packages not supported by make build system");
    default:
      return std::unexpected("Unknown source type");
  }
}

std::expected<void, std::string> MakePackage::build(const std::filesystem::path& source_path) const
{
  auto actual_source_path = get_actual_source_path(source_path);

  // Check for Makefile
  if (!has_makefile(actual_source_path))
  {
    return std::unexpected("No Makefile found in source directory: " + actual_source_path.string());
  }

  // Execute make
  auto make_result = execute_make(actual_source_path);
  if (!make_result)
  {
    return make_result;
  }

  // Setup build flags
  setup_build_flags(actual_source_path);

  return {};
}

bool MakePackage::has_makefile(const std::filesystem::path& source_path) const
{
  return std::filesystem::exists(source_path / "Makefile") ||
         std::filesystem::exists(source_path / "makefile") ||
         std::filesystem::exists(source_path / "GNUmakefile");
}

std::expected<void, std::string> MakePackage::execute_make(
    const std::filesystem::path& source_path) const
{
  std::string make_command = get_make_command();

  auto result = execute_command(make_command, source_path);
  if (!result)
  {
    return std::unexpected("Make build failed: " + result.error());
  }

  return {};
}

void MakePackage::setup_build_flags(const std::filesystem::path& source_path) const
{
  // Set up include paths
  std::vector<std::string> include_paths;

  // Common include directories
  if (std::filesystem::exists(source_path / "include"))
  {
    include_paths.push_back((source_path / "include").string());
  }
  if (std::filesystem::exists(source_path / "src"))
  {
    include_paths.push_back((source_path / "src").string());
  }

  const_cast<MakePackage*>(this)->set_include_paths(std::move(include_paths));

  // Set up library paths
  std::vector<std::string> library_paths;
  library_paths.push_back(source_path.string());

  // Common library output directories
  if (std::filesystem::exists(source_path / "lib"))
  {
    library_paths.push_back((source_path / "lib").string());
  }
  if (std::filesystem::exists(source_path / "build"))
  {
    library_paths.push_back((source_path / "build").string());
  }

  const_cast<MakePackage*>(this)->set_library_paths(std::move(library_paths));

  // Set up libraries (scan for built libraries)
  std::vector<std::string> libraries;
  for (const auto& lib_path : library_paths)
  {
    if (!std::filesystem::exists(lib_path))
    {
      continue;
    }

    for (const auto& entry : std::filesystem::directory_iterator(lib_path))
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
          libraries.push_back(lib_name);
        }
      }
    }
  }

  const_cast<MakePackage*>(this)->set_link_flags(std::move(libraries));
}

std::string MakePackage::get_make_command() const
{
  std::ostringstream cmd;
  cmd << "make";

  // Add build arguments
  for (const auto& arg : info_.build_args)
  {
    cmd << " " << arg;
  }

  // Add parallel build if not specified
  bool has_parallel = false;
  for (const auto& arg : info_.build_args)
  {
    if (arg.find("-j") != std::string::npos)
    {
      has_parallel = true;
      break;
    }
  }

  if (!has_parallel)
  {
    cmd << " -j$(nproc)";
  }

  return cmd.str();
}

}  // namespace cppup::buildsystems::make

// Register the package type
REGISTER_PACKAGE_TYPE(cppup::buildsystems::make::MakePackage, "make");