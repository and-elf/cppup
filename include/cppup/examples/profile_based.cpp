/**
 * @file profile_based.cpp
 * @brief Example showing profile-based configuration
 */

#include <cppup/configuration.hpp>

using namespace cppup::configuration;

extern "C" BuildConfiguration configure()
{
  return BuildConfiguration{
      // Base configuration (applies to all profiles)
      .toolchain = Toolchain{"clang-17"},
      .packages  = {Package{"boost"}},
      .sources   = {"src/*.cpp"},
      .binaries  = {Binary{"myapp", {"src/main.cpp"}}},

      // Profile-specific configurations
      .profiles = {
          // Debug profile
          Profile{"debug"} {
              .packages      = {Package{"debug-tools"}},
              .compile_flags = {Flag{"-g"}, Flag{"-O0"}, Flag{"-fsanitize=address"}},
              .link_flags    = {Flag{"-fsanitize=address"}},
              .definitions   = {Definition{"DEBUG", "1"}, Definition{"LOG_LEVEL", "DEBUG"}}},

          // Release profile
          Profile{"release"} {
              .compile_flags = {Flag{"-O3"}, Flag{"-DNDEBUG"}, Flag{"-flto"}},
              .link_flags =
                  {
                      Flag{"-flto"}, Flag{"-s"}  // Strip symbols
                  },
              .definitions = {Definition{"RELEASE", "1"}, Definition{"LOG_LEVEL", "ERROR"}}},

          // Profiling profile
          Profile{"profiling"} {
              .compile_flags = {Flag{"-pg"}, Flag{"-O2"}, Flag{"-g"}},
              .link_flags    = {Flag{"-pg"}},
              .definitions   = {Definition{"PROFILING", "1"}, Definition{"LOG_LEVEL", "INFO"}}},

          // Testing profile
          Profile{"testing"} {
              .packages      = {Package{"catch2"}, Package{"benchmark"}},
              .compile_flags = {Flag{"-g"}, Flag{"-O1"}, Flag{"-fno-omit-frame-pointer"}},
              .definitions = {Definition{"TESTING", "1"}, Definition{"ENABLE_BENCHMARKS", "1"}}}}};
}