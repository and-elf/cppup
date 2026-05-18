#include <cppup/configuration.hpp>

using namespace cppup::configuration;

extern "C" BuildConfiguration configure()
{
  BuildConfiguration config;

  config.toolchain     = Toolchain{"g++"};
  config.compile_flags = {Flag{"-Wall"},      Flag{"-Wextra"}, Flag{"-Wpedantic"},
                          Flag{"-std=c++23"}, Flag{"-O2"},     Flag{"-DNDEBUG"}, Flag{"-fPIC"}};
  config.include_paths = {"include",       "src",          "src/cli",
                          "src/core/cli",  "src/core/cli/commands",
                          "src/core/configuration"};
  config.link_flags    = {Flag{"-lsqlite3"}, Flag{"-lcrypto"}, Flag{"-ldl"}, Flag{"-pthread"}};

  config.libraries.push_back(Library{"cppup_config",
                                     {"src/core/configuration/compiler.cpp",
                                      "src/core/configuration/compile_commands.cpp",
                                      "src/core/configuration/loader.cpp",
                                      "src/core/configuration/validation.cpp",
                                      "src/core/configuration/package_resolver.cpp",
                                      "src/core/configuration/toolchain_resolver.cpp",
                                      "src/core/configuration/profile_processor.cpp",
                                      "src/core/configuration/build_step_executor.cpp"},
                                     LibraryType::Static});

  config.libraries.push_back(Library{"cppup_dependency",
                                     {"src/core/dependency/database.cpp"},
                                     LibraryType::Static});

  config.libraries.push_back(
      Library{"cppup_build", {"src/core/build/cache.cpp"}, LibraryType::Static});

  config.libraries.push_back(
      Library{"cppup_cli",
              {"src/core/cli/cli_application.cpp", "src/core/cli/logger.cpp",
               "src/core/cli/commands.cpp", "src/core/cli/commands/common.cpp",
               "src/core/cli/commands/init.cpp", "src/core/cli/commands/build.cpp",
               "src/core/cli/commands/compile_commands_cmd.cpp",
               "src/core/cli/commands/test.cpp", "src/core/cli/commands/format.cpp",
               "src/core/cli/commands/package.cpp", "src/core/cli/commands/module.cpp",
               "src/core/cli/commands/toolchain.cpp", "src/core/cli/commands/plugin.cpp"},
              LibraryType::Static});

  config.binaries.push_back(Binary{"cppup", {"src/main.cpp"}});

  config.definitions = {Definition{"CPPUP_VERSION", "0.1.0"},
                        Definition{"CPPUP_BUILD_TYPE", "Release"}};

  return config;
}
