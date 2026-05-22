#include "make_package.hpp"

#include <filesystem>
#include <sstream>

using cppup::configuration::Package;
using cppup::configuration::PackageInfo;

namespace cppup::buildsystems::make
{

MakePackage::MakePackage(PackageInfo info) : info_(std::move(info)) {}

void MakePackage::ensure_source_package() const
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

std::expected<std::filesystem::path, std::string> MakePackage::resolve_source() const
{
  ensure_source_package();
  if (!source_package_)
  {
    return std::unexpected("Failed to create source package");
  }
  return source_package_->resolve_source();
}

std::expected<void, std::string> MakePackage::build(const std::filesystem::path& source_path) const
{
  auto actual_source_path = cppup::package::utils::get_actual_source_path(source_path, info_);

  if (!has_makefile(actual_source_path))
  {
    return std::unexpected("No Makefile found in source directory: " + actual_source_path.string());
  }

  auto make_result = execute_make(actual_source_path);
  if (!make_result)
  {
    return make_result;
  }

  setup_build_flags(actual_source_path);
  return {};
}

bool MakePackage::has_makefile(const std::filesystem::path& source_path)
{
  return std::filesystem::exists(source_path / "Makefile") ||
         std::filesystem::exists(source_path / "makefile") ||
         std::filesystem::exists(source_path / "GNUmakefile");
}

std::expected<void, std::string> MakePackage::execute_make(
    const std::filesystem::path& source_path) const
{
  if (!command_executor_)
  {
    return std::unexpected("No command executor available");
  }

  std::string const make_command = get_make_command();

  auto result =
      cppup::package::utils::execute_command(*command_executor_, make_command, source_path);
  if (!result)
  {
    return std::unexpected("Make build failed: " + result.error());
  }
  return {};
}

void MakePackage::setup_build_flags(const std::filesystem::path& source_path) const
{
  include_paths_.clear();
  if (std::filesystem::exists(source_path / "include"))
  {
    include_paths_.push_back((source_path / "include").string());
  }
  if (std::filesystem::exists(source_path / "src"))
  {
    include_paths_.push_back((source_path / "src").string());
  }

  library_paths_.clear();
  library_paths_.push_back(source_path.string());
  if (std::filesystem::exists(source_path / "lib"))
  {
    library_paths_.push_back((source_path / "lib").string());
  }
  if (std::filesystem::exists(source_path / "build"))
  {
    library_paths_.push_back((source_path / "build").string());
  }

  link_flags_.clear();
  for (const auto& lib_path : library_paths_)
  {
    if (!std::filesystem::exists(lib_path))
    {
      continue;
    }
    for (const auto& entry : std::filesystem::directory_iterator(lib_path))
    {
      if (!entry.is_regular_file())
      {
        continue;
      }
      const auto extension = entry.path().extension().string();
      if (extension == ".a" || extension == ".so" || extension == ".dll" || extension == ".lib")
      {
        auto lib_name = entry.path().stem().string();
        if (lib_name.starts_with("lib"))
        {
          lib_name = lib_name.substr(3);
        }
        link_flags_.push_back("-l" + lib_name);
      }
    }
  }
}

std::string MakePackage::get_make_command() const
{
  std::ostringstream cmd;
  cmd << "make";

  for (const auto& arg : info_.build_args)
  {
    cmd << " " << arg;
  }

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
