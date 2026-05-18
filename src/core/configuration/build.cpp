#include <cppup/configuration.hpp>

using namespace cppup::configuration;

extern "C" BuildConfiguration configure()
{
  BuildConfiguration config;
  config.libraries.push_back(Library{
      .name       = "cppup_config",
      .sources    = {"compiler.cpp", "compile_commands.cpp", "loader.cpp", "validation.cpp",
                     "package_resolver.cpp", "toolchain_resolver.cpp", "profile_processor.cpp",
                     "build_step_executor.cpp"},
      .type       = LibraryType::Static,
      .link_flags = {Flag{"-ldl"}},
  });

  config.tests.push_back(Test{"test_profile", {"tests/test_profile.cpp"}});
  config.tests.back().link_flags = {Flag{"-lgtest"}, Flag{"-lgtest_main"}, Flag{"-lpthread"}};

  return config;
}
