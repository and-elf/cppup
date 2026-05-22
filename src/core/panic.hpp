#pragma once

#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <string_view>

namespace cppup
{

[[noreturn]] inline void panic(std::string_view     msg,
                               std::source_location loc = std::source_location::current()) noexcept
{
  std::fputs("cppup: panic at ", stderr);
  std::fputs(loc.file_name(), stderr);
  std::fputc(':', stderr);
  // NOLINTNEXTLINE(modernize-use-std-print) -- keep panic path non-throwing under noexcept
  std::fprintf(stderr, "%u", static_cast<unsigned>(loc.line()));
  std::fputs(": ", stderr);
  std::fwrite(msg.data(), sizeof(char), msg.size(), stderr);
  std::fputc('\n', stderr);
  std::abort();
}

}  // namespace cppup

// `cond` is materialized into a local bool before negation so that callers
// passing a logical expression like `a || b` don't trip
// readability-simplify-boolean-expr's DeMorgan check on the expanded
// `if (!(a || b))` at every CPPUP_CHECK site.
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage) -- needs lazy `msg` evaluation
#define CPPUP_CHECK(cond, msg)                               \
  do                                                         \
  {                                                          \
    const bool cppup_check_passed = static_cast<bool>(cond); \
    if (!cppup_check_passed) [[unlikely]]                    \
    {                                                        \
      ::cppup::panic(msg);                                   \
    }                                                        \
  } while (0)
