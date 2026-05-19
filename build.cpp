#include <cppup/configuration.hpp>
#include <cstdlib>

using namespace cppup::configuration;

extern "C" BuildConfiguration configure()
{
  // Regenerate src/core/cli/commands/init_templates_data.hpp from
  // templates/init/** before configuring the build. The header is gitignored;
  // the cache will pick up any content change via header-dep tracking and
  // invalidate cppup_cli automatically when templates are edited.
  std::system("./scripts/embed_init_templates.sh >/dev/null");

  BuildConfiguration config;

  config.toolchain     = Toolchain{"g++"};
  config.compile_flags = {Flag{"-Wall"},
                          Flag{"-Wextra"},
                          Flag{"-Wpedantic"},
                          Flag{"-Werror"},
                          Flag{"-Wno-return-type-c-linkage"},
                          Flag{"-std=c++23"},
                          Flag{"-O2"},
                          Flag{"-g"},
                          Flag{"-DNDEBUG"},
                          Flag{"-fPIC"}};
  config.include_paths = {"include",
                          "src",
                          "src/cli",
                          "src/core/cli",
                          "src/core/cli/commands",
                          "src/core/configuration"};

  config.subprojects = {
      Subproject{.path = "src/core/configuration", .build_system = {}, .build_args = {}},
      Subproject{.path = "src/core/dependency", .build_system = {}, .build_args = {}},
      Subproject{
          .path         = "src/core/build",
          .build_system = {},
          .build_args   = {},
      },
      Subproject{.path = "src/core/cli", .build_system = {}, .build_args = {}},
  };

  config.binaries.push_back(
      Binary{.name = "cppup", .sources = {"src/main.cpp"}, .libraries = {"cppup_cli"}});

  config.definitions = {Definition{"CPPUP_VERSION", "0.1.0"},
                        Definition{"CPPUP_BUILD_TYPE", "Release"}};

  return config;
}
