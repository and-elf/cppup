#include <cppup/configuration.hpp>

using namespace cppup::configuration;

// No .cpp sources in this directory — public CLI headers only
// (CLI11.hpp, cli_application.hpp, logger.hpp). The CLI implementation
// lives in src/core/cli/.
extern "C" BuildConfiguration configure()
{
  BuildConfiguration config;
  config.include_paths = {".."};
  return config;
}
