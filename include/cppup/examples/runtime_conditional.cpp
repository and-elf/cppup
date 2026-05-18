/**
 * @file runtime_conditional.cpp
 * @brief Example showing runtime conditional configuration
 */

#include <cppup/configuration.hpp>

using namespace cppup::configuration;

CPPUP_CONFIGURE()
{
  BuildConfiguration config{.sources  = {"src/*.cpp"},
                            .binaries = {Binary{"myapp", {"src/main.cpp"}}}};

  // Note: Runtime conditionals would be processed by the build system
  // after loading the configuration. These are examples of how they
  // would be used if the build system populated the runtime fields.

  // Feature-based configuration
  when_feature(config, "openssl",
               [&]()
               {
                 config.packages.push_back(Package{"openssl"});
                 config.definitions.push_back(Definition{"HAVE_OPENSSL", "1"});
                 config.compile_flags.push_back(Flag{"-DUSE_OPENSSL"});
               });

  when_feature(config, "threading",
               [&]()
               {
                 config.link_flags.push_back(Flag{"-pthread"});
                 config.definitions.push_back(Definition{"HAVE_THREADING", "1"});
               });

  when_feature(config, "networking",
               [&]()
               {
                 config.packages.push_back(Package{"libcurl"});
                 config.definitions.push_back(Definition{"HAVE_NETWORKING", "1"});
               });

  // Environment-based configuration
  when_env(config, "DEBUG", "true",
           [&]()
           {
             config.compile_flags.insert(config.compile_flags.end(), {Flag{"-g"}, Flag{"-O0"}});
             config.definitions.push_back(Definition{"DEBUG_MODE", "1"});
           });

  when_env(config, "OPTIMIZATION", "O2", [&]() { config.compile_flags.push_back(Flag{"-O2"}); });

  when_env(config, "OPTIMIZATION", "O3", [&]() { config.compile_flags.push_back(Flag{"-O3"}); });

  when_env(config, "TARGET", "production",
           [&]()
           {
             config.definitions.push_back(Definition{"PROD_BUILD", "1"});
             config.compile_flags.push_back(Flag{"-DNDEBUG"});
           });

  // Check if environment variable exists (regardless of value)
  when_env_exists(
      config, "CI",
      [&]()
      {
        config.definitions.push_back(Definition{"CI_BUILD", "1"});
        config.compile_flags.push_back(Flag{"-Werror"});  // Treat warnings as errors in CI
      });

  // Multiple feature checks
  if (has_all_features(config, {"openssl", "networking"}))
  {
    config.definitions.push_back(Definition{"SECURE_NETWORKING", "1"});
  }

  if (has_any_feature(config, {"debug", "testing", "profiling"}))
  {
    config.definitions.push_back(Definition{"DEVELOPMENT_BUILD", "1"});
  }

  // Environment variable with default
  std::string log_level = get_env_or(config, "LOG_LEVEL", "INFO");
  config.definitions.push_back(Definition{"LOG_LEVEL", "\"" + log_level + "\""});

  return config;
}