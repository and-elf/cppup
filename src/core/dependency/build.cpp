#include <cppup/configuration.hpp>

using namespace cppup::configuration;

extern "C" BuildConfiguration configure()
{
  BuildConfiguration config;
  config.libraries.push_back(Library{
      .name       = "cppup_dependency",
      .sources    = {"database.cpp"},
      .type       = LibraryType::Static,
      .link_flags = {Flag{"-lsqlite3"}},
  });

  config.tests.push_back(Test{"test_dependency", {"test_dependency.cpp"}});
  config.tests.back().libraries  = {"cppup_dependency"};
  config.tests.back().link_flags = {Flag{"-lsqlite3"}};
  config.tests.back().framework  = "gtest";

  return config;
}
