#include <cppup/configuration.hpp>

using namespace cppup::configuration;

extern "C" BuildConfiguration configure()
{
  BuildConfiguration config;

  config.libraries.push_back(Library{
      "cppup_package_core", {"package_concept.cpp", "package_factory.cpp"}, LibraryType::Static});

  config.include_paths = {"../../.."};

  config.compile_flags = {Flag{"-std=c++23"}, Flag{"-Wall"}, Flag{"-Wextra"}};

  config.link_flags = {Flag{"-lcppup_config"}};

  return config;
}
