#pragma once

#include <optional>
#include <string>

namespace cppup::configuration
{

enum class Asan : unsigned char
{
  Off,
  On
};

enum class Coverage : unsigned char
{
  Off,
  On
};

enum class Verbose : unsigned char
{
  Off,
  On
};

enum class WithTests : unsigned char
{
  Off,
  On
};

/**
 * Runtime build toggles that affect compile + link flags.
 *
 * Passed through every layer (`cppup build`, `cppup test`, `cppup compile-commands`,
 * and `emit_compile_commands`) so the compile_commands.json clangd sees matches
 * what the build actually emits.
 */
struct BuildOptions
{
  Asan      asan       = Asan::Off;
  Coverage  coverage   = Coverage::Off;
  Verbose   verbose    = Verbose::Off;
  WithTests with_tests = WithTests::Off;
  // 0 = auto (std::thread::hardware_concurrency()); 1 = serial.
  unsigned jobs{};
  // CLI overrides for the active toolchain / profile. When unset, the
  // build resolves them from the persisted selection (cppup.lock) and
  // then from the configuration's defaults. Strings (not enums) because
  // both name spaces are user-defined.
  std::optional<std::string> toolchain = {};
  std::optional<std::string> profile   = {};
};

[[nodiscard]] constexpr bool enabled(Asan asan_option) noexcept
{
  return asan_option == Asan::On;
}
[[nodiscard]] constexpr bool enabled(Coverage coverage_option) noexcept
{
  return coverage_option == Coverage::On;
}
[[nodiscard]] constexpr bool enabled(Verbose verbose_option) noexcept
{
  return verbose_option == Verbose::On;
}
[[nodiscard]] constexpr bool enabled(WithTests with_tests_option) noexcept
{
  return with_tests_option == WithTests::On;
}

}  // namespace cppup::configuration
