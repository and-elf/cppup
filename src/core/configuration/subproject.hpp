#pragma once

#include <cstdint>
#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <system_error>
#include <vector>

namespace cppup::configuration
{

/**
 * Build system used by a subproject. Inferred from directory contents when
 * left unspecified on a Subproject.
 */
enum class BuildSystem : std::uint8_t
{
  Cppup,
  CMake,
  Make,
  HeaderOnly,
};

/**
 * A nested project the parent build depends on. Each subproject is built by
 * its own build system; the parent merges the resulting libraries/binaries
 * into its target set.
 */
struct Subproject
{
  std::string                path;                        // relative to the parent build.cpp
  std::optional<BuildSystem> build_system = {};           // nullopt = infer from `path`
  std::vector<std::string>   build_args   = {};           // forwarded to CMake/Make invocations
  std::string                build_file   = "build.cpp";  // Cppup config file name
};

/**
 * Probe `dir` and decide which build system handles it. Priority:
 * `cppup_build_file` (default "build.cpp") > CMakeLists.txt >
 * Makefile/GNUmakefile > headers-only. If multiple markers are present the
 * call fails — the caller must set Subproject::build_system explicitly.
 */
inline std::expected<BuildSystem, std::string> infer_build_system(
    const std::filesystem::path& dir, const std::string& cppup_build_file = "build.cpp")
{
  std::error_code ec;
  if (!std::filesystem::is_directory(dir, ec))
  {
    return std::unexpected("not a directory: " + dir.string());
  }

  const bool has_build_cpp  = std::filesystem::exists(dir / cppup_build_file, ec);
  const bool has_cmakelists = std::filesystem::exists(dir / "CMakeLists.txt", ec);
  const bool has_makefile   = std::filesystem::exists(dir / "Makefile", ec) ||
                            std::filesystem::exists(dir / "GNUmakefile", ec);

  const int marker_count = static_cast<int>(has_build_cpp) + static_cast<int>(has_cmakelists) +
                           static_cast<int>(has_makefile);
  if (marker_count > 1)
  {
    return std::unexpected("ambiguous build system in " + dir.string() +
                           ": multiple markers present, set Subproject::build_system explicitly");
  }
  if (has_build_cpp)
  {
    return BuildSystem::Cppup;
  }
  if (has_cmakelists)
  {
    return BuildSystem::CMake;
  }
  if (has_makefile)
  {
    return BuildSystem::Make;
  }

  // No explicit marker — fall back to header-only if the directory contains
  // headers but no compilable source files.
  bool has_source = false;
  bool has_header = false;
  for (const auto& entry : std::filesystem::directory_iterator(dir, ec))
  {
    if (!entry.is_regular_file(ec))
    {
      continue;
    }
    const auto ext = entry.path().extension().string();
    if (ext == ".cpp" || ext == ".cc" || ext == ".cxx" || ext == ".c")
    {
      has_source = true;
    }
    else if (ext == ".hpp" || ext == ".h" || ext == ".hh" || ext == ".hxx")
    {
      has_header = true;
    }
  }
  if (has_header && !has_source)
  {
    return BuildSystem::HeaderOnly;
  }

  return std::unexpected("no recognizable build system in " + dir.string());
}

}  // namespace cppup::configuration
