/**
 * Example build.cpp demonstrating the cppup configuration API.
 *
 * Shows a small project with a static library, an executable that links it,
 * a plain-binary unit test, and a release profile.
 */

#include <cppup/configuration.hpp>

using namespace cppup::configuration;

extern "C" BuildConfiguration configure()
{
  BuildConfiguration config;

  config.toolchain               = Toolchain{"g++"};
  config.toolchain->cxx_standard = CxxStandard::Cxx23;
  config.toolchain->warnings     = WarningLevel::Strict;

  config.compile_flags = {Flag{"-O2"}};

  config.libraries = {
      Library{
          .name = "simple_lib", .sources = {"src/lib/simple_lib.cpp"}, .type = LibraryType::Static},
  };

  config.binaries = {
      Binary{.name = "simple_app", .sources = {"src/main.cpp"}, .libraries = {"simple_lib"}}};

  config.tests                  = {Test{"unit_tests", {"tests/test_main.cpp"}}};
  config.tests.back().libraries = {"simple_lib"};

  config.profiles = {
      Profile{.name          = "release",
              .compile_flags = {Flag{"-O3"}, Flag{"-march=native"}},
              .definitions   = {Definition{"NDEBUG"}}},
  };

  return config;
}
