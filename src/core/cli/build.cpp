#include <cppup/configuration.hpp>

using namespace cppup::configuration;

extern "C" BuildConfiguration configure()
{
  BuildConfiguration config;
  config.libraries.push_back(Library{
      .name       = "cppup_cli",
      .sources    = {"cli_application.cpp", "logger.cpp", "commands.cpp", "commands/common.cpp",
                     "commands/init.cpp", "commands/build.cpp", "commands/compile_commands_cmd.cpp",
                     "commands/test.cpp", "commands/format.cpp", "commands/tidy.cpp",
                     "commands/source_selection.cpp", "commands/package.cpp", "commands/module.cpp",
                     "commands/toolchain.cpp", "commands/plugin.cpp"},
      .type       = LibraryType::Static,
      .link_flags = {Flag{"-pthread"}},
      .libraries  = {"cppup_config", "cppup_build", "cppup_dependency"},
  });

  config.tests.push_back(Test{"test_source_selection", {"commands/test_source_selection.cpp"}});
  config.tests.back().libraries  = {"cppup_cli", "cppup_config", "cppup_build", "cppup_dependency"};
  config.tests.back().link_flags = {Flag{"-lsqlite3"}, Flag{"-lgtest"}, Flag{"-lgtest_main"},
                                    Flag{"-lpthread"}, Flag{"-ldl"}};

  config.tests.push_back(Test{"test_init", {"commands/test_init.cpp"}});
  config.tests.back().libraries  = {"cppup_cli", "cppup_config", "cppup_build", "cppup_dependency"};
  config.tests.back().link_flags = {Flag{"-lsqlite3"}, Flag{"-lgtest"}, Flag{"-lgtest_main"},
                                    Flag{"-lpthread"}, Flag{"-ldl"}};

  return config;
}
