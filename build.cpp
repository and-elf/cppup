#include <cppup/configuration.hpp>

using namespace cppup::configuration;

extern "C" BuildConfiguration configure()
{
  BuildConfiguration config;

  config.toolchain     = Toolchain{"g++"};
  config.compile_flags = {Flag{"-Wall"}, Flag{"-Wextra"},  Flag{"-Wpedantic"}, Flag{"-std=c++23"},
                          Flag{"-O2"},   Flag{"-DNDEBUG"}, Flag{"-fPIC"}};
  config.include_paths = {"include",
                          "src",
                          "src/cli",
                          "src/core/cli",
                          "src/core/cli/commands",
                          "src/core/configuration"};

  config.subprojects = {
      Subproject{.path = "src/core/configuration"},
      Subproject{.path = "src/core/dependency"},
      Subproject{.path = "src/core/build"},
      Subproject{.path = "src/core/cli"},
  };

  config.binaries.push_back(
      Binary{.name = "cppup", .sources = {"src/main.cpp"}, .libraries = {"cppup_cli"}});

  config.definitions = {Definition{"CPPUP_VERSION", "0.1.0"},
                        Definition{"CPPUP_BUILD_TYPE", "Release"}};

  return config;
}
