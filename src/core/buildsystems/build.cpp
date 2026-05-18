#include <cppup/configuration.hpp>

using namespace cppup::configuration;

// Aggregator: each build system has its own build.cpp under
// cmake/, cppup/, make/, header_only/. No direct sources at this level.
extern "C" BuildConfiguration configure()
{
  BuildConfiguration config;
  config.include_paths = {"../../.."};
  return config;
}
