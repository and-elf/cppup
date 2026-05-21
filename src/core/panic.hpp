#pragma once

#include <cstdlib>
#include <print>
#include <source_location>
#include <string_view>

namespace cppup
{

[[noreturn]] inline void panic(std::string_view     msg,
                               std::source_location loc = std::source_location::current())
{
  std::println(stderr, "cppup: panic at {}:{}: {}", loc.file_name(), loc.line(), msg);
  std::abort();
}

}  // namespace cppup

#define CPPUP_CHECK(cond, msg) \
  do                           \
  {                            \
    if (!(cond)) [[unlikely]]  \
    {                          \
      ::cppup::panic(msg);     \
    }                          \
  } while (0)
