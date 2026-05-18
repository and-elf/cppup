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
  return config;
}
