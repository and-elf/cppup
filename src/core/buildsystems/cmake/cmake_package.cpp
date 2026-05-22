#include "cmake_package.hpp"

#include <filesystem>
#include <sstream>

using namespace cppup::configuration;

namespace cppup::buildsystems::cmake
{

CMakePackage::CMakePackage(PackageInfo info) : info_(std::move(info)) {}

std::expected<std::filesystem::path, std::string> CMakePackage::resolve_source() const
{
  ensure_source_package();

  if (!source_package_)
  {
    return std::unexpected("Failed to create source package");
  }

  return source_package_->resolve_source();
}

void CMakePackage::ensure_source_package() const
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

std::expected<void, std::string> CMakePackage::build(const std::filesystem::path& source_path) const
{
  auto actual_source_path = cppup::package::utils::get_actual_source_path(source_path, info_);

  // Check for CMakeLists.txt
  if (!has_cmake_file(actual_source_path))
  {
    return std::unexpected("No CMakeLists.txt found in source directory: " +
                           actual_source_path.string());
  }

  // Configure CMake
  auto configure_result = configure_cmake(actual_source_path);
  if (!configure_result)
  {
    return configure_result;
  }

  // Build with CMake
  auto build_result = build_cmake(actual_source_path);
  if (!build_result)
  {
    return build_result;
  }

  // Setup build flags
  auto build_path = actual_source_path / "build";
  if (std::filesystem::exists(build_path))
  {
    setup_build_flags(actual_source_path, build_path);
  }

  return {};
}

bool CMakePackage::has_cmake_file(const std::filesystem::path& source_path)
{
  return std::filesystem::exists(source_path / "CMakeLists.txt");
}

std::expected<void, std::string> CMakePackage::configure_cmake(
    const std::filesystem::path& source_path) const
{
  if (!command_executor_)
  {
    return std::unexpected("No command executor available");
  }

  // Create build directory
  auto build_dir = source_path / "build";
  std::filesystem::create_directories(build_dir);

  std::string const configure_command = get_cmake_configure_command(source_path);

  auto result =
      cppup::package::utils::execute_command(*command_executor_, configure_command, source_path);
  if (!result)
  {
    return std::unexpected("CMake configure failed: " + result.error());
  }

  return {};
}

std::expected<void, std::string> CMakePackage::build_cmake(
    const std::filesystem::path& source_path) const
{
  if (!command_executor_)
  {
    return std::unexpected("No command executor available");
  }

  std::string build_command = "cmake --build build";

  // Add parallel build if not specified
  bool has_parallel = false;
  for (const auto& arg : info_.build_args)
  {
    if (arg.find("-j") != std::string::npos || arg.find("--parallel") != std::string::npos)
    {
      has_parallel = true;
      break;
    }
  }

  if (!has_parallel)
  {
    build_command += " --parallel";
  }

  auto result =
      cppup::package::utils::execute_command(*command_executor_, build_command, source_path);
  if (!result)
  {
    return std::unexpected("CMake build failed: " + result.error());
  }

  return {};
}

void CMakePackage::setup_build_flags(const std::filesystem::path& source_path,
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

  // CMake generated headers
  if (std::filesystem::exists(build_path / "include"))
  {
    include_paths_.push_back((build_path / "include").string());
  }

  // Set up library paths
  library_paths_.clear();
  library_paths_.push_back(build_path.string());

  // Common CMake library output directories
  if (std::filesystem::exists(build_path / "lib"))
  {
    library_paths_.push_back((build_path / "lib").string());
  }
  if (std::filesystem::exists(build_path / "Debug"))
  {
    library_paths_.push_back((build_path / "Debug").string());
  }
  if (std::filesystem::exists(build_path / "Release"))
  {
    library_paths_.push_back((build_path / "Release").string());
  }

  // Set up libraries (scan for built libraries)
  link_flags_.clear();
  for (const auto& lib_path : library_paths_)
  {
    if (!std::filesystem::exists(lib_path))
    {
      continue;
    }

    for (const auto& entry : std::filesystem::recursive_directory_iterator(lib_path))
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
}

std::string CMakePackage::get_cmake_configure_command(
    const std::filesystem::path& /*source_path*/) const
{
  std::ostringstream cmd;
  cmd << "cmake -S . -B build";

  // Add build arguments
  for (const auto& arg : info_.build_args)
  {
    cmd << " " << arg;
  }

  // Default to Release build if not specified
  bool has_build_type = false;
  for (const auto& arg : info_.build_args)
  {
    if (arg.find("CMAKE_BUILD_TYPE") != std::string::npos)
    {
      has_build_type = true;
      break;
    }
  }

  if (!has_build_type)
  {
    cmd << " -DCMAKE_BUILD_TYPE=Release";
  }

  return cmd.str();
}

}  // namespace cppup::buildsystems::cmake