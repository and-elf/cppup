#include <cppup/configuration.hpp>

using namespace cppup::configuration;

extern "C" BuildConfiguration configure()
{
  BuildConfiguration config;

  // toml++ single-header is vendored at src/toml++/toml.hpp. Add the
  // src/ root so `#include <toml++/toml.hpp>` resolves. The plugin C ABI
  // header at include/cppup/plugin/abi.h is reached via the top-level
  // include/ path already configured by the parent build.
  config.include_paths = {"../.."};

  config.libraries.push_back(Library{
      .name    = "cppup_plugin",
      .sources = {"manifest.cpp", "vtable_support.cpp", "descriptor_validation.cpp"},
      .type    = LibraryType::Static,
  });

  config.tests.push_back(Test{"test_manifest", {"test_manifest.cpp"}});
  config.tests.back().libraries  = {"cppup_plugin"};
  config.tests.back().link_flags = {Flag{"-lgtest"}, Flag{"-lgtest_main"}, Flag{"-lpthread"}};

  config.tests.push_back(Test{"test_vtable_support", {"test_vtable_support.cpp"}});
  config.tests.back().libraries  = {"cppup_plugin"};
  config.tests.back().link_flags = {Flag{"-lgtest"}, Flag{"-lgtest_main"}, Flag{"-lpthread"}};

  config.tests.push_back(Test{"test_descriptor_validation", {"test_descriptor_validation.cpp"}});
  config.tests.back().libraries  = {"cppup_plugin"};
  config.tests.back().link_flags = {Flag{"-lgtest"}, Flag{"-lgtest_main"}, Flag{"-lpthread"}};

  return config;
}
