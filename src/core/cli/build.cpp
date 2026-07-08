#include <cppup/configuration.hpp>

using namespace cppup::configuration;

extern "C" BuildConfiguration configure()
{
  BuildConfiguration config;
  config.libraries.push_back(Library{
      .name       = "cppup_cli",
      .sources    = {"cli_application.cpp",
                     "commands.cpp",
                     "commands/init.cpp",
                     "commands/build.cpp",
                     "commands/clean.cpp",
                     "commands/compile_commands_cmd.cpp",
                     "commands/test.cpp",
                     "commands/test_dispatch.cpp",
                     "commands/format.cpp",
                     "commands/tidy.cpp",
                     "commands/source_selection.cpp",
                     "commands/package.cpp",
                     "commands/package_source_plugin_bridge.cpp",
                     "commands/package_source_registry.cpp",
                     "commands/progress_sink.cpp",
                     "commands/ref_parser.cpp",
                     "commands/download_helpers.cpp",
                     "commands/sync_progress.cpp",
                     "commands/lockfile.cpp",
                     "commands/module.cpp",
                     "commands/toolchain.cpp",
                     "commands/plugin.cpp",
                     "commands/update.cpp",
                     "commands/selection_resolver.cpp",
                     "commands/coverage_parser.cpp",
                     "commands/registry.cpp",
                     "commands/selection_resolver.cpp",
                     "commands/subproject_runner.cpp",
                     "commands/toolchain_probe.cpp",
                     "commands/install_paths.cpp"},
      .type       = LibraryType::Static,
      .link_flags = {Flag{"-pthread"}, Flag{"-ldl"}},
      .libraries  = {"cppup_config", "cppup_build", "cppup_dependency", "cppup_logger_console",
                     "cppup_plugin", "cppup_packages", "cppup_buildsystems"},
  });

  const auto add_test = [&](const char* name, const char* source)
  {
    config.tests.push_back(Test{name, {source}});
    config.tests.back().libraries  = {"cppup_cli", "cppup_config", "cppup_build",
                                      "cppup_dependency"};
    config.tests.back().link_flags = {Flag{"-lsqlite3"}, Flag{"-ldl"}};
    config.tests.back().framework  = "gtest";
  };

  add_test("test_source_selection", "commands/test_source_selection.cpp");
  add_test("test_init", "commands/test_init.cpp");
  add_test("test_update", "commands/test_update.cpp");
  add_test("test_lockfile", "commands/test_lockfile.cpp");
  add_test("test_ref_parser", "commands/test_ref_parser.cpp");
  add_test("test_install_paths", "commands/test_install_paths.cpp");
  add_test("test_package_user_scope", "commands/test_package_user_scope.cpp");
  add_test("test_toolchain_user_scope", "commands/test_toolchain_user_scope.cpp");
  add_test("test_coverage_parser", "commands/test_coverage_parser.cpp");
  add_test("test_subproject_runner", "commands/test_subproject_runner.cpp");
  add_test("test_toolchain_probe", "commands/test_toolchain_probe.cpp");
  add_test("test_test_dispatch", "commands/test_test_dispatch.cpp");
  add_test("test_download_helpers", "commands/test_download_helpers.cpp");

  return config;
}
