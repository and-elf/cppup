#include <cppup/configuration.hpp>

using namespace cppup::configuration;

extern "C" BuildConfiguration configure()
{
  BuildConfiguration config;

  config.libraries.push_back(
      Library{"cppup_logger_console", {"console_logger.cpp"}, LibraryType::Static});

  config.include_paths = {"..", "../../../.."};

  config.compile_flags = {Flag{"-std=c++23"}, Flag{"-Wall"}, Flag{"-Wextra"}};

  return config;
}
