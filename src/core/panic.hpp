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
  struct PanicSink
  {
    bool write_failed = false;

    void write_literal(const char* text) noexcept
    {
      if (write_failed)
      {
        return;
      }
      if (std::fputs(text, stderr) == EOF)
      {
        write_failed = true;
      }
    }

    void write_char(char input) noexcept
    {
      if (write_failed)
      {
        return;
      }
      if (std::fputc(input, stderr) == EOF)
      {
        write_failed = true;
      }
    }

    void write_line(unsigned line) noexcept
    {
      if (write_failed)
      {
        return;
      }
      // NOLINTNEXTLINE(modernize-use-std-print) -- keep panic path non-throwing under noexcept
      if (std::fprintf(stderr, "%u", line) < 0)
      {
        write_failed = true;
      }
    }

    void write_message(std::string_view text) noexcept
    {
      if (write_failed)
      {
        return;
      }
      if (std::fwrite(text.data(), sizeof(char), text.size(), stderr) != text.size())
      {
        write_failed = true;
      }
    }
  } sink;

  sink.write_literal("cppup: panic at ");
  sink.write_literal(loc.file_name());
  sink.write_char(':');
  sink.write_line(static_cast<unsigned>(loc.line()));
  sink.write_literal(": ");
  sink.write_message(msg);
  sink.write_char('\n');
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
