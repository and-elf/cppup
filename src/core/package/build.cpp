#include <cppup/configuration.hpp>

using namespace cppup::configuration;

extern "C" BuildConfiguration configure()
{
  BuildConfiguration config;

  // Shared utility functions used by every package-source plugin
  // (utils::execute_command, utils::download_file, etc. declared in
  // package_concept.hpp). The legacy PackageFactory + per-source
  // switch in package_factory.cpp has been superseded by the plugin
  // registry and is no longer built.
  config.libraries.push_back(Library{
      .name    = "cppup_package_utils",
      .sources = {"package_concept.cpp"},
      .type    = LibraryType::Static,
  });

  // Aggregated package-source library combining all built-in source
  // types as static plugins. TODO: gate each source-type subset via
  // CPPUP_WITH_GIT / CPPUP_WITH_DIRECTORY / ... flags once the build
  // system can forward Subproject::build_args into sub-build.cpps.
  config.libraries.push_back(Library{
      .name      = "cppup_packages",
      .sources   = {"packages.cpp", "git/git_package.cpp", "git/git_plugin.cpp",
                    "directory/directory_package.cpp", "directory/directory_plugin.cpp",
                    "archive/archive_package.cpp", "archive/archive_plugin.cpp",
                    "http/http_package.cpp", "http/http_plugin.cpp", "registry/registry_package.cpp",
                    "registry/registry_plugin.cpp"},
      .type      = LibraryType::Static,
      .libraries = {"cppup_plugin", "cppup_package_utils"},
  });

  config.include_paths = {"../../.."};
  config.compile_flags = {Flag{"-std=c++23"}, Flag{"-Wall"}, Flag{"-Wextra"}};

  const std::vector<Flag> test_links = {Flag{"-lgtest"}, Flag{"-lgtest_main"}, Flag{"-lpthread"},
                                        Flag{"-ldl"}};

  config.tests.push_back(Test{"test_git_plugin", {"git/test_git_plugin.cpp"}});
  config.tests.back().libraries  = {"cppup_packages", "cppup_plugin"};
  config.tests.back().link_flags = test_links;

  config.tests.push_back(Test{"test_package_plugins", {"test_package_plugins.cpp"}});
  config.tests.back().libraries  = {"cppup_packages", "cppup_plugin"};
  config.tests.back().link_flags = test_links;

  return config;
}
