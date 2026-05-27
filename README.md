# cppup

cppup is a cross-platform C++ project manager and build system inspired by Cargo. Projects are configured in C++ — you write a `configure()` function in `build.cpp` and cppup compiles it, loads it, and drives the build from the returned `BuildConfiguration`.

It focuses on:
- Build-system-agnostic package resolution (cppup-native, CMake, Make, header-only)
- Project configuration written in real C++ (`build.cpp`), not a bespoke DSL
- Practical defaults for modern C++ workflows (C++20/23/26, sanitizers, coverage)
- Incremental and cache-friendly builds, with a `compile_commands.json` exporter for clangd

## Quick Start

```bash
# scaffold a new project (TTY: asks which optional templates to include)
cppup init my_project

# add a dependency by git URL + branch (or --version, --tag, --commit, --url, --dir)
cppup package add --name fmt --git https://github.com/fmtlib/fmt.git --branch 11.0.2

# build and test
cppup build
cppup test

# emit compile_commands.json so clangd / clang-tidy can see your build
cppup compile-commands
```

## Build and Bootstrap cppup

cppup uses a two-stage bootstrap:

1. `bootstrap.sh` compiles a **slim** `cppup_bootstrap` binary that only knows two commands: `build` (compile the full cppup from this source tree into `./build/cppup`) and `update` (download a released full cppup binary into `~/.cppup/bin/`).
2. The slim binary then either downloads a prebuilt cppup or builds the full one from source.

### Linux and macOS

```bash
./bootstrap.sh                              # → bootstrap_build/cppup_bootstrap
./bootstrap_build/cppup_bootstrap update    # install prebuilt cppup (fast)
# — or —
./bootstrap_build/cppup_bootstrap build     # build full cppup from source → build/cppup
```

`bootstrap.sh` takes no arguments. It also installs the tracked `.githooks/` (gitleaks + `cppup format --check` + `cppup tidy` on every commit) via `scripts/setup-hooks.sh` if the working copy is a git clone.

### Windows

```bat
bootstrap.bat
```

The slim binary is written to `bootstrap_build\cppup_bootstrap.exe`.

## Configuration API

Your project defines an `extern "C" configure()` function that returns a `BuildConfiguration`. The function lives in `build.cpp` at the project root.

```cpp
#include <cppup/configuration.hpp>

using namespace cppup::configuration;

extern "C" BuildConfiguration configure()
{
  BuildConfiguration config;

  config.toolchain               = Toolchain{"g++"};
  config.toolchain->cxx_standard = CxxStandard::Cxx23;
  config.toolchain->warnings     = WarningLevel::Werror;

  config.compile_flags = {Flag{"-O2"}, Flag{"-g"}};
  config.include_paths = {"include", "src"};

  config.binaries = {
      Binary{.name = "app", .sources = {"src/main.cpp"}},
  };

  return config;
}
```

`Toolchain` carries the dialect and warning policy as strong enums (`CxxStandard::{Cxx17,Cxx20,Cxx23,Cxx26}`, `WarningLevel::{None,Standard,Strict,Werror}`); the toolchain expander maps them to the right family-specific flag strings (`-std=c++23 -Wall -Wextra -Wpedantic -Werror` on gcc/clang, the MSVC equivalents elsewhere). Anything compiler-specific that doesn't fit goes verbatim in `Toolchain::extra_flags`.

For convenience, the legacy flag-list helpers (`warnings::extra()`, `cpp_standard::cpp23()`, `optimization::speed()`, `debug_profile()`, `release_profile()`, `test_profile("catch2")`) are still available — see [src/core/configuration/cppup_config.hpp](src/core/configuration/cppup_config.hpp).

## Packages

Add dependencies through the CLI (recommended) or by populating `config.packages` directly. cppup supports five source types — git, local directory, HTTP download, tar/zip archive, and the package registry — and four build-system backends: cppup-native, CMake, Make, and header-only. The backend is normally inferred from the package's directory contents; pass `--build-system` to override when more than one marker is present.

```bash
cppup package add --name fmt    --git https://github.com/fmtlib/fmt.git --branch 11.0.2
cppup package add --name boost  --version 1.84.0                          # registry
cppup package add --name mylib  --dir ../mylib                            # local directory
cppup package add --name catch2 --url https://.../catch2.tar.gz           # archive
cppup package add --name foo    --git https://... --build-system cmake    # force backend

cppup package list
cppup package remove fmt
```

## Subprojects

Nested projects (with their own build system) merge their libraries and binaries into the parent build. Each subproject's build system is inferred from its directory contents — `build.cpp` → cppup, `CMakeLists.txt` → CMake, `Makefile`/`GNUmakefile` → Make, headers only → header-only — and can be overridden explicitly:

```cpp
config.subprojects = {
    Subproject{.path = "src/core/configuration"},
    Subproject{.path = "src/core/dependency"},
    Subproject{.path = "src/core/build"},
    Subproject{.path = "src/core/cli"},
};
```

## CLI Commands

```
cppup init <name>                    Scaffold a new project.
    --full                              Include all optional templates
    --minimal                           Base layout only, no prompts
    --with-vscode / --with-devcontainer / --with-docker / --with-gitlab-ci
                                        Per-template opt-in (skips the TTY prompt)

cppup build                          Build libraries and binaries.
    --asan                              Enable AddressSanitizer
    --coverage                          Instrument with gcov coverage flags
    --with-tests                        Also compile test binaries
    -V, --verbose                       Echo compile/link commands as they run
    -j, --jobs <N>                      Parallel compile jobs (0 = auto)

cppup test                           Run tests (compiles them if needed).
    --asan, --coverage

cppup compile-commands               Emit compile_commands.json for clangd/tooling.
    --asan, --coverage                  Mirror those flags in the emitted commands

cppup clean                          Remove build artifacts.
    --all                               Also wipe .cppup/packages, toolchains, plugins, bin

cppup format [files...]              Format with clang-format.
    --check                             Verify formatting without modifying files

cppup tidy [files...]                Run clang-tidy (needs compile_commands.json).
    --fix                               Apply suggested fixes in place

cppup package add|list|remove        Manage project packages.
cppup toolchain add|list|remove|select
cppup plugin add|list|remove
cppup module add <name>

cppup update                         Install the latest released cppup binary.
    --check                             Print running + latest version, no install
    --version <tag>                     Install a specific tag
    --install-dir <path>                Override install dir (default: $HOME/.cppup/bin)

cppup --version
```

## Environment Variables

- `CXX` — compiler override during `./bootstrap.sh` (default `g++`).
- `CPPUP_RELEASE_REPO` — override the GitHub `owner/repo` that `cppup update` pulls from (useful for forks).
- `CPPUP_SKIP_HOOKS=1` — skip the entire pre-commit hook for one commit.
- `CPPUP_SKIP_GITLEAKS=1` / `CPPUP_SKIP_FORMAT=1` / `CPPUP_SKIP_TIDY=1` — skip the individual checks in `.githooks/pre-commit`.

## Troubleshooting

Compiler not found or too old:

```bash
export CXX=clang++
./bootstrap.sh
```

cppup is bootstrapped with C++23 and builds itself with C++26. Use GCC 13+ or a recent Clang.

`cppup tidy` fails with "compile_commands.json missing":

```bash
cppup compile-commands       # or run a full `cppup build` first
```

## Development

- Source layout: `src/core/{configuration,dependency,build,buildsystems,package,cli,logger}/`. Each module is consumed as a subproject from the top-level [build.cpp](build.cpp).
- Unit tests live next to their modules under `src/core/**/tests/` and are compiled when you pass `--with-tests` or run `cppup test`.
- Example projects: [examples/simple_project/](examples/simple_project/) and [test_build_project/](test_build_project/).
- Pre-commit hooks (`.githooks/pre-commit`): gitleaks secret scan, `cppup format --check`, `cppup tidy` — wired up automatically by `bootstrap.sh` via [scripts/setup-hooks.sh](scripts/setup-hooks.sh).
- Generated headers: [scripts/embed_init_templates.sh](scripts/embed_init_templates.sh) bakes `templates/init/**` into `init_templates_data.hpp`, and [scripts/amalgamate_configuration_header.sh](scripts/amalgamate_configuration_header.sh) produces the single-header `cppup/configuration.hpp` that user projects `#embed`. Both are re-run by [build.cpp](build.cpp) on every configure.

## Additional References

- CLI command docs: [src/core/cli/README.md](src/core/cli/README.md)
- Configuration module docs: [src/core/configuration/README.md](src/core/configuration/README.md)
- Package backends: [src/core/package/README.md](src/core/package/README.md), [src/core/dependency/README.md](src/core/dependency/README.md)
- Specs: [.kiro/specs/cli-commands/](.kiro/specs/cli-commands/) and [.kiro/specs/configuration-api/](.kiro/specs/configuration-api/)

## License

MIT, with a non-binding Beerware addendum — see [LICENSE](LICENSE). If we meet some day and you think this is worth it, you can buy the author a beer.
