#pragma once

// Build system feature flags
// These can be controlled at compile time to enable/disable specific build systems

#ifndef CPPUP_NO_CMAKE
#define CPPUP_ENABLE_CMAKE 1
#endif

#ifndef CPPUP_NO_MAKE
#define CPPUP_ENABLE_MAKE 1
#endif

#ifndef CPPUP_NO_HEADER_ONLY
#define CPPUP_ENABLE_HEADER_ONLY 1
#endif

#ifndef CPPUP_NO_MESON
#define CPPUP_ENABLE_MESON 1
#endif

#ifndef CPPUP_NO_AUTOTOOLS
#define CPPUP_ENABLE_AUTOTOOLS 1
#endif

// Always enable cppup build system
#define CPPUP_ENABLE_CPPUP 1

namespace cppup::configuration
{

/**
 * Check if a build system feature is enabled at compile time
 */
constexpr bool is_build_system_enabled(const char* build_system)
{
  if (std::string_view(build_system) == "cppup")
  {
    return CPPUP_ENABLE_CPPUP;
  }
  if (std::string_view(build_system) == "cmake")
  {
    return CPPUP_ENABLE_CMAKE;
  }
  if (std::string_view(build_system) == "make")
  {
    return CPPUP_ENABLE_MAKE;
  }
  if (std::string_view(build_system) == "header_only")
  {
    return CPPUP_ENABLE_HEADER_ONLY;
  }
  if (std::string_view(build_system) == "meson")
  {
    return CPPUP_ENABLE_MESON;
  }
  if (std::string_view(build_system) == "autotools")
  {
    return CPPUP_ENABLE_AUTOTOOLS;
  }
  return false;
}

}  // namespace cppup::configuration