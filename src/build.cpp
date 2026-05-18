#include <cppup/configuration.hpp>

using namespace cppup::configuration;

// Aggregator for the src/ tree: per-subdirectory build.cpp files
// (core/configuration, core/package, core/buildsystems/*, etc.) declare
// their own libraries. The cppup binary itself is wired in the repo-root
// build.cpp.
extern "C" BuildConfiguration configure()
{
  BuildConfiguration config;
  config.include_paths = {"..", "../include"};
  return config;
}
