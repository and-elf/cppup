#pragma once

#include <string>
#include <string_view>

#include "outputs.hpp"

namespace cppup::configuration
{

// Compile-time platform detection
#ifdef _WIN32
constexpr std::string_view target_os = "windows";
#elifdef __linux__
constexpr std::string_view target_os = "linux";
#elifdef __APPLE__
constexpr std::string_view target_os = "macos";
#else
constexpr std::string_view target_os = "unknown";
#endif

#if defined(_M_X64) || defined(__x86_64__)
constexpr std::string_view target_arch = "x86_64";
#elif defined(_M_ARM64) || defined(__aarch64__)
constexpr std::string_view target_arch = "arm64";
#else
constexpr std::string_view target_arch = "unknown";
#endif

// Cross-platform current-host tag combining the compile-time OS and
// architecture as "<os>-<arch>" (e.g. "linux-x86_64", "macos-arm64",
// "windows-x86_64"). Derived from the compiler's target macros, so it works
// on Windows, Linux and macOS without POSIX `uname()`. Used to select the
// matching release asset when self-updating.
[[nodiscard]] inline std::string current_platform_tag()
{
  return std::string{target_os} + "-" + std::string{target_arch};
}

// Compile-time platform queries
[[nodiscard]] constexpr bool is_windows() noexcept
{
  return target_os == "windows";
}

[[nodiscard]] constexpr bool is_linux() noexcept
{
  return target_os == "linux";
}

[[nodiscard]] constexpr bool is_macos() noexcept
{
  return target_os == "macos";
}

[[nodiscard]] constexpr bool is_x86_64() noexcept
{
  return target_arch == "x86_64";
}

[[nodiscard]] constexpr bool is_arm64() noexcept
{
  return target_arch == "arm64";
}

// True when `toolchain` names a Windows-targeting cross- or native
// compiler: any mingw triple, any `*-windows-*` LLVM triple, MSVC `cl` /
// `clang-cl`. Drives the executable/library extension helpers below so
// `build.cpp` can describe targets without baking in the host's OS.
[[nodiscard]] inline bool toolchain_targets_windows(std::string_view toolchain) noexcept
{
  return toolchain.contains("mingw") || toolchain.contains("windows") || toolchain == "cl" ||
         toolchain == "cl.exe" || toolchain == "clang-cl" || toolchain == "clang-cl.exe";
}

[[nodiscard]] inline bool toolchain_targets_macos(std::string_view toolchain) noexcept
{
  return toolchain.contains("apple-darwin") || toolchain.contains("-darwin");
}

// Filename suffix the linker stamps onto executables for the given
// toolchain. Empty for ELF/Mach-O hosts; ".exe" for any Windows target.
// cppup appends this to planned `output_path`s so the build cache's
// `exists()` probe matches what the linker actually wrote.
[[nodiscard]] inline std::string_view executable_extension(std::string_view toolchain) noexcept
{
  return toolchain_targets_windows(toolchain) ? std::string_view{".exe"} : std::string_view{};
}

// Library extension for the active toolchain's target platform.
[[nodiscard]] inline std::string_view library_extension(LibraryType      type,
                                                        std::string_view toolchain) noexcept
{
  if (toolchain_targets_windows(toolchain))
  {
    return (type == LibraryType::Static) ? ".lib" : ".dll";
  }
  if (toolchain_targets_macos(toolchain))
  {
    return (type == LibraryType::Static) ? ".a" : ".dylib";
  }
  return (type == LibraryType::Static) ? ".a" : ".so";
}

// Compile-time conditional configuration helpers
template <typename Func>
constexpr void when_windows(Func&& func)
{
  if constexpr (is_windows())
  {
    std::forward<Func>(func)();
  }
}

template <typename Func>
constexpr void when_linux(Func&& func)
{
  if constexpr (is_linux())
  {
    std::forward<Func>(func)();
  }
}

template <typename Func>
constexpr void when_macos(Func&& func)
{
  if constexpr (is_macos())
  {
    std::forward<Func>(func)();
  }
}

template <typename Func>
constexpr void when_x86_64(Func&& func)
{
  if constexpr (is_x86_64())
  {
    std::forward<Func>(func)();
  }
}

template <typename Func>
constexpr void when_arm64(Func&& func)
{
  if constexpr (is_arm64())
  {
    std::forward<Func>(func)();
  }
}

}  // namespace cppup::configuration