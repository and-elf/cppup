#include "toolchain_flags.hpp"

namespace cppup::configuration
{

namespace
{

const char* cxx_std_flag(CxxStandard s)
{
  switch (s)
  {
    case CxxStandard::Cxx17:
      return "-std=c++17";
    case CxxStandard::Cxx20:
      return "-std=c++20";
    case CxxStandard::Cxx23:
      return "-std=c++23";
    case CxxStandard::Cxx26:
      return "-std=c++26";
    case CxxStandard::Unspecified:
      break;
  }
  return nullptr;
}

void append_warning_flags(std::vector<std::string>& out, WarningLevel level)
{
  switch (level)
  {
    case WarningLevel::None:
      return;
    case WarningLevel::Werror:
      out.emplace_back("-Werror");
      [[fallthrough]];
    case WarningLevel::Strict:
      out.emplace_back("-Wall");
      out.emplace_back("-Wextra");
      out.emplace_back("-Wpedantic");
      return;
    case WarningLevel::Standard:
      out.emplace_back("-Wall");
      return;
  }
}

}  // namespace

std::vector<std::string> dialect_flags(const Toolchain& toolchain)
{
  std::vector<std::string> out;
  if (const auto* std_flag = cxx_std_flag(toolchain.cxx_standard))
  {
    out.emplace_back(std_flag);
  }
  append_warning_flags(out, toolchain.warnings);
  for (const auto& f : toolchain.extra_flags)
  {
    out.emplace_back(f);
  }
  return out;
}

}  // namespace cppup::configuration
