#pragma once

#include <string_view>

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

#ifdef _M_X64 || defined(__x86_64__)
constexpr std::string_view target_arch = "x86_64";
#elif defined(_M_ARM64) || defined(__aarch64__)
constexpr std::string_view target_arch = "arm64";
#else
constexpr std::string_view target_arch = "unknown";
#endif

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