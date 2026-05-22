#include <cppup/configuration.hpp>

using namespace cppup::configuration;

extern "C" BuildConfiguration configure()
{
  BuildConfiguration config;

  config.libraries.push_back(Library{
      .name      = "cppup_logger_console",
      .sources   = {"console_logger.cpp", "console_logger_plugin.cpp"},
      .type      = LibraryType::Static,
      .libraries = {"cppup_plugin"},
  });

  config.include_paths = {"..", "../../../.."};

  config.compile_flags = {Flag{"-std=c++23"}, Flag{"-Wall"}, Flag{"-Wextra"}};

  const std::vector<Flag> test_links = {Flag{"-lgtest"}, Flag{"-lgtest_main"}, Flag{"-lpthread"},
                                        Flag{"-ldl"}};

  config.tests.push_back(Test{"test_console_logger_plugin", {"test_console_logger_plugin.cpp"}});
  config.tests.back().libraries  = {"cppup_logger_console", "cppup_plugin"};
  config.tests.back().link_flags = test_links;

  return config;
}
