#include <cppup/configuration.hpp>

using namespace cppup::configuration;

extern "C" BuildConfiguration configure()
{
  BuildConfiguration config;
  config.include_paths = {"../.."};

  config.libraries.push_back(Library{
      .name    = "cppup_test_frameworks",
      .sources = {"gtest_plugin.cpp"},
      .type    = LibraryType::Static,
  });

  // Hardcoded -lgtest links here remain until the plugin can dogfood
  // itself; that's the last slice of this feature.
  const std::vector<Flag> test_links = {Flag{"-lgtest"}, Flag{"-lgtest_main"}, Flag{"-lpthread"}};

  config.tests.push_back(Test{"test_gtest_plugin", {"test_gtest_plugin.cpp"}});
  config.tests.back().libraries  = {"cppup_test_frameworks", "cppup_plugin"};
  config.tests.back().link_flags = test_links;

  return config;
}
