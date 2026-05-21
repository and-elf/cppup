#include <cppup/configuration.hpp>

using namespace cppup::configuration;

extern "C" BuildConfiguration configure()
{
  BuildConfiguration config;

  // toml++ single-header is vendored at src/toml++/toml.hpp. Add the
  // src/ root so `#include <toml++/toml.hpp>` resolves.
  config.include_paths = {"../.."};

  config.libraries.push_back(Library{
      .name    = "cppup_plugin",
      .sources = {"manifest.cpp", "vtable_support.cpp"},
      .type    = LibraryType::Static,
  });

  config.tests.push_back(Test{"test_manifest", {"test_manifest.cpp"}});
  config.tests.back().libraries  = {"cppup_plugin"};
  config.tests.back().link_flags = {Flag{"-lgtest"}, Flag{"-lgtest_main"}, Flag{"-lpthread"}};

  config.tests.push_back(Test{"test_vtable_support", {"test_vtable_support.cpp"}});
  config.tests.back().libraries  = {"cppup_plugin"};
  config.tests.back().link_flags = {Flag{"-lgtest"}, Flag{"-lgtest_main"}, Flag{"-lpthread"}};

  return config;
}
