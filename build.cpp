#include <cppup/configuration.hpp>
#include <cstdlib>

using namespace cppup::configuration;
using namespace cppup::configuration::package_helpers;

extern "C" BuildConfiguration configure()
{
  // Regenerate src/core/cli/commands/init_templates_data.hpp from
  // templates/init/** before configuring the build. The header is gitignored;
  // the cache will pick up any content change via header-dep tracking and
  // invalidate cppup_cli automatically when templates are edited.
  std::system("./scripts/embed_init_templates.sh >/dev/null");
  // Materialize build/generated/cppup/configuration.hpp before configuring;
  // src/core/cli/commands/embedded_configuration_header.hpp pulls it in via
  // #embed so user projects can `#include <cppup/configuration.hpp>` after
  // `cppup build` writes the bytes into their .cppup/include/cppup/.
  std::system("./scripts/amalgamate_configuration_header.sh >/dev/null");

  BuildConfiguration config;

  // Default toolchain — overridden by `cppup toolchain select <name>` or
  // `--toolchain` on the CLI. The when_toolchain blocks below customize
  // dialect/warning/extra flags for the active selection. `when_*`
  // helpers fire inside configure() because the CLI exports the resolved
  // selection into the environment before this DSO is loaded.
  config.toolchain =
      Toolchain{std::string{active_toolchain().empty() ? "g++" : active_toolchain()}};

  // Language + warning defaults are shared across gcc/clang front-ends;
  // the toolchain expander emits the right family-specific spelling.
  config.toolchain->cxx_standard = CxxStandard::Cxx26;
  config.toolchain->warnings     = WarningLevel::Werror;
  config.toolchain->extra_flags  = {"-Wno-return-type-c-linkage"};

  when_toolchain("clang++", [&] { config.toolchain->extra_flags.emplace_back("-stdlib=libc++"); });

  // Defaults match the historical release-style build (-O2, NDEBUG, -fPIC
  // for the .so config DSO). when_profile blocks below switch them.
  config.compile_flags = {Flag{"-O2"}, Flag{"-g"}, Flag{"-DNDEBUG"}, Flag{"-fPIC"}};

  when_profile("debug", [&] { config.compile_flags = {Flag{"-O0"}, Flag{"-g"}, Flag{"-fPIC"}}; });
  when_profile("release",
               [&]
               {
                 // -O3 + stripped (-Wl,-s) gives a slim, optimized binary.
                 // Drop -g so the strip pass actually saves space rather
                 // than tossing already-compiled debug sections.
                 config.compile_flags = {Flag{"-O3"}, Flag{"-DNDEBUG"}, Flag{"-fPIC"}};
                 config.link_flags.push_back(Flag{"-Wl,-s"});
               });

  config.include_paths = {"include",
                          "src",
                          "src/cli",
                          "src/core/cli",
                          "src/core/cli/commands",
                          "src/core/configuration"};

  // Dogfood: gtest is fetched as a source package and built by the
  // builtin gtest test-framework plugin. Tests reference this by
  // `framework = "gtest"` instead of carrying hardcoded `-lgtest`
  // link flags. The package roundtrips through `cppup.lock` so a fresh
  // `git clone && cppup build` reproduces.
  config.test_frameworks.push_back(TestFramework{
      .name   = "gtest",
      .plugin = "gtest",
  });

  config.subprojects = {
      Subproject{.path = "src/core/configuration", .build_system = {}, .build_args = {}},
      Subproject{.path = "src/core/dependency", .build_system = {}, .build_args = {}},
      Subproject{
          .path         = "src/core/build",
          .build_system = {},
          .build_args   = {},
      },
      Subproject{.path = "src/core/logger/console", .build_system = {}, .build_args = {}},
      Subproject{.path = "src/core/plugin", .build_system = {}, .build_args = {}},
      Subproject{.path = "src/core/test_frameworks", .build_system = {}, .build_args = {}},
      Subproject{.path = "src/core/package", .build_system = {}, .build_args = {}},
      Subproject{.path = "src/core/buildsystems", .build_system = {}, .build_args = {}},
      Subproject{.path = "src/core/cli", .build_system = {}, .build_args = {}},
  };

  config.binaries.push_back(Binary{.name      = "cppup",
                                   .sources   = {"src/main.cpp"},
                                   .libraries = {"cppup_cli", "cppup_test_frameworks"}});

  config.definitions = {
      Definition{"CPPUP_MAJOR_VERSION", "1"}, Definition{"CPPUP_MINOR_VERSION", "0"},
      Definition{"CPPUP_PATCH_VERSION", "0"}, Definition{"CPPUP_BUILD_TYPE", "Release"}};

  return config;
}
