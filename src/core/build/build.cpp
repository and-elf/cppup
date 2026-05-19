#include <cppup/configuration.hpp>

using namespace cppup::configuration;

extern "C" BuildConfiguration configure()
{
  BuildConfiguration config;
  config.libraries.push_back(Library{
      .name       = "cppup_build",
      .sources    = {"cache.cpp"},
      .type       = LibraryType::Static,
      .link_flags = {Flag{"-lsqlite3"}, Flag{"-lcrypto"}},
  });

  config.tests.push_back(Test{"test_cache", {"test_cache.cpp"}});
  config.tests.back().libraries  = {"cppup_build", "cppup_dependency"};
  config.tests.back().link_flags = {Flag{"-lsqlite3"}, Flag{"-lcrypto"}, Flag{"-lgtest"},
                                    Flag{"-lgtest_main"}, Flag{"-lpthread"}};

  return config;
}
