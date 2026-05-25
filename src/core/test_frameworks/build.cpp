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

  config.tests.push_back(Test{"test_gtest_plugin", {"test_gtest_plugin.cpp"}});
  config.tests.back().libraries = {"cppup_test_frameworks", "cppup_plugin"};
  config.tests.back().framework = "gtest";

  return config;
}
