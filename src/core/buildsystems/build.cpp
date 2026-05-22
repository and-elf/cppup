#include <cppup/configuration.hpp>

using namespace cppup::configuration;

extern "C" BuildConfiguration configure()
{
  BuildConfiguration config;

  // Aggregated build-system library combining all built-in build
  // systems as static plugins. Same pattern as cppup_packages; per
  // build-system flag gating is a future enhancement.
  config.libraries.push_back(Library{
      .name      = "cppup_buildsystems",
      .sources   = {"cmake/cmake_package.cpp", "cmake/cmake_plugin.cpp", "make/make_package.cpp",
                    "make/make_plugin.cpp", "header_only/header_only_package.cpp",
                    "header_only/header_only_plugin.cpp", "cppup/cppup_package.cpp",
                    "cppup/cppup_plugin.cpp"},
      .type      = LibraryType::Static,
      .libraries = {"cppup_plugin", "cppup_package_utils", "cppup_packages"},
  });

  config.include_paths = {"../../.."};
  config.compile_flags = {Flag{"-std=c++23"}, Flag{"-Wall"}, Flag{"-Wextra"}};

  const std::vector<Flag> test_links = {Flag{"-lgtest"}, Flag{"-lgtest_main"}, Flag{"-lpthread"},
                                        Flag{"-ldl"}};

  config.tests.push_back(Test{"test_buildsystem_plugins", {"test_buildsystem_plugins.cpp"}});
  config.tests.back().libraries  = {"cppup_buildsystems", "cppup_packages", "cppup_plugin"};
  config.tests.back().link_flags = test_links;

  return config;
}
