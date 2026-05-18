#include <cppup/configuration.hpp>

using namespace cppup::configuration;

// Aggregator: logger backends live in subdirectories (console/, ...).
// The LoggerType concept and shared LogLevel/LogConfig types are in
// logger_concept.hpp.
extern "C" BuildConfiguration configure()
{
  BuildConfiguration config;
  config.include_paths = {".", "../../.."};
  return config;
}
