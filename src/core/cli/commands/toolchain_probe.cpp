#include "toolchain_probe.hpp"

#include <cstdlib>
#include <filesystem>
#include <set>
#include <string>

#include "../../configuration/platform.hpp"

namespace cppup::cli
{

namespace
{

namespace fs = std::filesystem;

bool is_regular_file_or_symlink(const fs::path& candidate) noexcept
{
  std::error_code error_code;
  const auto      status = fs::status(candidate, error_code);
  if (error_code)
  {
    return false;
  }
  return fs::is_regular_file(status);
}

}  // namespace

std::vector<ProbeHit> probe_toolchains(const std::vector<fs::path>&    search_dirs,
                                       const std::vector<std::string>& basenames)
{
  std::vector<ProbeHit> hits;
  hits.reserve(basenames.size());
  std::set<std::string> already_found;

  for (const auto& basename : basenames)
  {
    if (already_found.contains(basename))
    {
      continue;
    }
    for (const auto& dir : search_dirs)
    {
      std::error_code error_code;
      if (!fs::exists(dir, error_code) || error_code)
      {
        continue;
      }
      auto try_candidate = [&](const fs::path& candidate) -> bool
      {
        if (!is_regular_file_or_symlink(candidate))
        {
          return false;
        }
        hits.push_back(ProbeHit{.name = basename, .path = candidate});
        already_found.insert(basename);
        return true;
      };
      if (try_candidate(dir / basename))
      {
        break;
      }
      if (cppup::configuration::is_windows() && try_candidate(dir / (basename + ".exe")))
      {
        break;
      }
    }
  }
  return hits;
}

std::vector<fs::path> path_search_dirs()
{
  const char* raw = std::getenv("PATH");
  if (raw == nullptr || *raw == '\0')
  {
    return {};
  }
  const char            separator = cppup::configuration::is_windows() ? ';' : ':';
  std::vector<fs::path> dirs;
  std::string           current;
  for (const char* cursor = raw; *cursor != '\0'; ++cursor)
  {
    if (*cursor == separator)
    {
      if (!current.empty())
      {
        dirs.emplace_back(current);
        current.clear();
      }
    }
    else
    {
      current.push_back(*cursor);
    }
  }
  if (!current.empty())
  {
    dirs.emplace_back(current);
  }
  return dirs;
}

std::vector<std::string> default_compiler_basenames()
{
  return {"g++", "clang++", "gcc", "clang", "clang-cl", "cl"};
}

std::string_view missing_toolchain_hint() noexcept
{
  if constexpr (cppup::configuration::is_windows())
  {
    return "No C++ toolchain detected on PATH.\n"
           "  Install one of:\n"
           "    - LLVM/clang via `winget install LLVM.LLVM`\n"
           "    - Visual Studio Build Tools (provides cl.exe / clang-cl.exe)\n"
           "    - llvm-mingw or winlibs (portable, drop into PATH)\n"
           "  Toolchain-as-package install via `cppup package add` is planned; until then\n"
           "  cppup needs an external compiler to be reachable on PATH.";
  }
  else if constexpr (cppup::configuration::is_macos())
  {
    return "No C++ toolchain detected on PATH.\n"
           "  Install one of:\n"
           "    - Xcode Command Line Tools: `xcode-select --install`\n"
           "    - LLVM/clang via Homebrew: `brew install llvm`\n"
           "  Toolchain-as-package install via `cppup package add` is planned; until then\n"
           "  cppup needs an external compiler to be reachable on PATH.";
  }
  else
  {
    return "No C++ toolchain detected on PATH.\n"
           "  Install one of:\n"
           "    - Debian/Ubuntu: `sudo apt install g++` or `sudo apt install clang`\n"
           "    - Fedora/RHEL:   `sudo dnf install gcc-c++` or `sudo dnf install clang`\n"
           "    - Arch:          `sudo pacman -S gcc` or `sudo pacman -S clang`\n"
           "  Toolchain-as-package install via `cppup package add` is planned; until then\n"
           "  cppup needs an external compiler to be reachable on PATH.";
  }
}

}  // namespace cppup::cli
