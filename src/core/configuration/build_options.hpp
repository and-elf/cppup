#pragma once

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
  unsigned jobs = 0;
};

[[nodiscard]] constexpr bool enabled(Asan a) noexcept
{
  return a == Asan::On;
}
[[nodiscard]] constexpr bool enabled(Coverage c) noexcept
{
  return c == Coverage::On;
}
[[nodiscard]] constexpr bool enabled(Verbose v) noexcept
{
  return v == Verbose::On;
}
[[nodiscard]] constexpr bool enabled(WithTests w) noexcept
{
  return w == WithTests::On;
}

}  // namespace cppup::configuration
