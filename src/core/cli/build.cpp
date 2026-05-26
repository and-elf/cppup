#include <cppup/configuration.hpp>

using namespace cppup::configuration;

extern "C" BuildConfiguration configure()
{
  BuildConfiguration config;
  config.libraries.push_back(Library{
      .name    = "cppup_cli",
      .sources = {"cli_application.cpp", "commands.cpp", "commands/init.cpp", "commands/build.cpp",
                  "commands/clean.cpp", "commands/compile_commands_cmd.cpp", "commands/test.cpp",
                  "commands/format.cpp", "commands/tidy.cpp", "commands/source_selection.cpp",
                  "commands/package.cpp", "commands/lockfile.cpp", "commands/module.cpp",
                  "commands/toolchain.cpp", "commands/plugin.cpp", "commands/update.cpp",
                  "commands/registry.cpp", "commands/selection_resolver.cpp"},
      .type    = LibraryType::Static,
      .link_flags = {Flag{"-pthread"}, Flag{"-ldl"}},
      .libraries  = {"cppup_config", "cppup_build", "cppup_dependency", "cppup_logger_console",
                     "cppup_plugin", "cppup_packages", "cppup_buildsystems"},
  });

  config.tests.push_back(Test{"test_source_selection", {"commands/test_source_selection.cpp"}});
  config.tests.back().libraries  = {"cppup_cli", "cppup_config", "cppup_build", "cppup_dependency"};
  config.tests.back().link_flags = {Flag{"-lsqlite3"}, Flag{"-lgtest"}, Flag{"-lgtest_main"},
                                    Flag{"-lpthread"}, Flag{"-ldl"}};

  config.tests.push_back(Test{"test_init", {"commands/test_init.cpp"}});
  config.tests.back().libraries  = {"cppup_cli", "cppup_config", "cppup_build", "cppup_dependency"};
  config.tests.back().link_flags = {Flag{"-lsqlite3"}, Flag{"-lgtest"}, Flag{"-lgtest_main"},
                                    Flag{"-lpthread"}, Flag{"-ldl"}};

  config.tests.push_back(Test{"test_update", {"commands/test_update.cpp"}});
  config.tests.back().libraries  = {"cppup_cli", "cppup_config", "cppup_build", "cppup_dependency"};
  config.tests.back().link_flags = {Flag{"-lsqlite3"}, Flag{"-lgtest"}, Flag{"-lgtest_main"},
                                    Flag{"-lpthread"}, Flag{"-ldl"}};

  config.tests.push_back(Test{"test_lockfile", {"commands/test_lockfile.cpp"}});
  config.tests.back().libraries  = {"cppup_cli", "cppup_config", "cppup_build", "cppup_dependency"};
  config.tests.back().link_flags = {Flag{"-lsqlite3"}, Flag{"-lgtest"}, Flag{"-lgtest_main"},
                                    Flag{"-lpthread"}, Flag{"-ldl"}};

  return config;
}
