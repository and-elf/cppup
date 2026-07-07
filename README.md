# cppup

cppup is a cross-platform C++ project manager and build system inspired by Cargo and uv. Projects are configured in C++ — you write a `configure()` function in `build.cpp` and cppup compiles it, loads it, and drives the build from the returned `BuildConfiguration`.

It focuses on:
- Build-system-agnostic package resolution (cppup-native, CMake, Make, header-only)
- Project configuration written in real C++ (`build.cpp`), not a bespoke DSL
- Reproducible builds via a project-root `cppup.lock` (materialized by `cppup sync`)
- A plugin ABI for build systems, package sources, and loggers — extensible without re-linking the host
- Practical defaults for modern C++ (C++20/23/26, sanitizers, coverage) and a `compile_commands.json` exporter for clangd

## Quick Start

```bash
# scaffold a new project (TTY: asks which optional templates to include)
cppup init my_project

# add a dependency (today only git and local directory sources actually fetch)
cppup package add --name fmt --git https://github.com/fmtlib/fmt.git --branch 11.0.2

# regenerate cppup.lock from build.cpp, then sync .cppup/packages/ from the lockfile
cppup lock
cppup sync

# build and test (build never reaches over the network; run `cppup sync`
# first if any locked package is unmaterialized)
cppup build
cppup test

# emit compile_commands.json so clangd / clang-tidy can see your build
cppup compile-commands
```

A clean checkout of a project with a committed `cppup.lock` needs an explicit `cppup sync` once to materialize packages, then `cppup build` thereafter — `build` never fetches over the network and fails fast (listing the missing packages) if any locked entry is not yet materialized.

## Build and Bootstrap cppup

cppup uses a two-stage bootstrap:

1. `bootstrap.sh` compiles a **slim** `cppup_bootstrap` binary that knows three commands: `sync` (materialize packages from `cppup.lock` into `.cppup/packages/`), `build` (compile the full cppup from this source tree into `./build/cppup`), and `update` (download a released full cppup binary into `~/.cppup/bin/`).
2. The slim binary then either downloads a prebuilt cppup or syncs packages and builds the full one from source.

### Linux and macOS

```bash
./bootstrap.sh                              # → bootstrap_build/cppup_bootstrap
./bootstrap_build/cppup_bootstrap update    # install prebuilt cppup (fast)
# — or —
./bootstrap_build/cppup_bootstrap sync      # materialize packages from cppup.lock
./bootstrap_build/cppup_bootstrap build     # build full cppup from source → build/cppup
```

`bootstrap.sh build` and `bootstrap.bat build` chain `sync` automatically before `build` for the one-shot path.

`bootstrap.sh` takes no arguments. It probes the compiler at `$CXX` (default `g++`) for C++23 support, then compiles the slim binary with `-std=c++26`. It also installs the tracked `.githooks/` (gitleaks + `cppup format --check` + `cppup tidy` on every commit) via `scripts/setup-hooks.sh` if the working copy is a git clone.

### Windows

```bat
bootstrap.bat
```

The slim binary is written to `bootstrap_build\cppup_bootstrap.exe`. The Windows bootstrap currently compiles with `-std=c++20`; the full binary still requires C++26 via a recent MSVC or clang-cl.

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

  // Profiles selected by `cppup build --profile X` or `cppup profile select X`.
  config.profiles = {
      Profile{.name = "debug",   .compile_flags = {Flag{"-O0"}, Flag{"-g"}}},
      Profile{.name = "release", .compile_flags = {Flag{"-O3"}}, .definitions = {Definition{"NDEBUG"}}},
  };

  // Adjust based on the resolved toolchain/profile/platform.
  when_toolchain("clang++", [&] { config.compile_flags.push_back(Flag{"-stdlib=libc++"}); });
  when_profile  ("release",  [&] { config.compile_flags.push_back(Flag{"-flto"}); });
  when_linux    (           [&] { config.link_flags.push_back(Flag{"-pthread"}); });

  return config;
}
```

`Toolchain` carries the dialect and warning policy as strong enums (`CxxStandard::{Cxx17,Cxx20,Cxx23,Cxx26}`, `WarningLevel::{None,Standard,Strict,Werror}`); the toolchain expander maps them to family-specific flags (`-std=c++23 -Wall -Wextra -Wpedantic -Werror` on gcc/clang, the MSVC equivalents elsewhere). Anything compiler-specific that doesn't fit goes verbatim in `Toolchain::extra_flags`.

`BuildConfiguration` also carries `test_frameworks` (registered test runners — see [docs/plugin_api.md](docs/plugin_api.md)), `subprojects`, `build_steps` (custom steps with dependencies), and a `target_os` / `target_arch` / `environment` / `features` set populated by the host before `configure()` runs.

The `when_*` helpers come in two families:

| Helper | Resolved | Source |
|---|---|---|
| `when_windows`, `when_linux`, `when_macos`, `when_x86_64`, `when_arm64` | Compile-time (host of `build.cpp`) | [platform.hpp](src/core/configuration/platform.hpp) |
| `when_toolchain(name, fn)`, `when_profile(name, fn)` | Runtime; reads `CPPUP_ACTIVE_TOOLCHAIN` / `CPPUP_ACTIVE_PROFILE` exported by the CLI | [runtime.hpp](src/core/configuration/runtime.hpp) |
| `when_feature(config, name, fn)`, `when_env(config, var, value, fn)`, `when_env_exists(config, var, fn)` | Runtime; reads `config.features` / `config.environment` populated by the host | [runtime.hpp](src/core/configuration/runtime.hpp) |

For convenience, the legacy flag-list helpers (`warnings::extra()`, `cpp_standard::cpp23()`, `optimization::speed()`, `debug_profile()`, `release_profile()`, `test_profile("catch2")`) are still available — see [src/core/configuration/cppup_config.hpp](src/core/configuration/cppup_config.hpp).

Full details: [src/core/configuration/README.md](src/core/configuration/README.md).

## Packages

Add dependencies through the CLI or by populating `config.packages` directly. cppup defines six source types and four build-system backends:

| Source | CLI form | Status |
|---|---|---|
| Git | `--git URL [--branch B \| --tag T \| --commit C]` | Working (fetch + checkout, cached) |
| Local directory | `--dir PATH` | Working |
| Tar archive | `--tar URL` | Placeholder fetch (creates empty dir; see deferred section in [docs/packages.md](docs/packages.md)) |
| Zip archive | `--zip URL` | Placeholder fetch |
| HTTP file | `--url URL` | Placeholder fetch |
| Registry | `--name NAME --version V` (no `--git`/`--dir`/etc.) | Stub — returns an error today |

Backends — cppup-native (`build.cpp`), CMake (`CMakeLists.txt`), Make (`Makefile` / `GNUmakefile`), header-only — are inferred from the package's directory contents. If more than one marker is present cppup errors out and you pass `--build-system {cppup,cmake,make,header-only}` to disambiguate.

**Short form** — one positional, ref shape inferred:

```bash
cppup add github:fmtlib/fmt@11.0.2          # github shorthand
cppup add gitlab:group/proj@v1              # gitlab shorthand
cppup add https://github.com/fmtlib/fmt.git # explicit git URL, optional @<ref> suffix
cppup add git@github.com:fmtlib/fmt.git     # ssh-style git URL
cppup add ./vendor/mylib                    # local directory
cppup add fmt                                # bare name → registry placeholder
cppup add https://example.com/foo.tar.gz    # archive URL (placeholder fetch)
cppup add github:foo/bar --name custom_name # override the inferred name
cppup add ./mylib --user                    # install into $XDG_DATA_HOME/cppup/
```

`cppup add` dispatches the ref through `RefParserRegistry`; plugins can register additional parsers for ref shapes they own (`conan:fmt/11`, `s3://...`, etc.).

**Long form** — explicit fields, useful for scripting:

```bash
cppup package add --name fmt    --git https://github.com/fmtlib/fmt.git --branch 11.0.2
cppup package add --name mylib  --dir ../mylib                            # local directory
cppup package add --name foo    --git https://... --build-system cmake    # force backend
cppup package add --name fmt    --git https://... --user                  # install into $XDG_DATA_HOME/cppup/

cppup package list                    # both project and user scope, tagged
cppup package remove fmt
cppup package lock                    # alias: cppup lock
cppup package sync                    # alias: cppup sync
```

### Lockfile (`cppup.lock`)

`cppup.lock` is a small line-based file at the project root that pins everything needed to reproduce the build. It captures the closure of `config.packages` (with transitive `dependencies` walked from each `PackageInfo`) plus the active selection (`selected_toolchain`, `selected_profile`, `selected_registry`). Commit it.

- `cppup lock` regenerates the package section from `build.cpp` (deterministic byte output).
- `cppup sync` materializes `.cppup/packages/<name>/` from the lockfile; idempotent. Fetches run in parallel up to `--jobs N` (default: hardware_concurrency). It also prints a one-line hint if a newer released cppup is available (like `pip`/`uv`); the check is best-effort and never fails or delays the sync, and can be disabled with `CPPUP_NO_VERSION_CHECK=1`.
- `cppup build` auto-runs sync when `cppup.lock` is present.
- `cppup toolchain select`, `cppup profile select`, `cppup registry set` only touch the selection keys; package entries are preserved.

`git_commit` and `checksum` exist in the schema but are written empty until a resolution pass pins them. Full schema, precedence rules, and team workflow: [docs/packages.md](docs/packages.md).

## Subprojects

Nested projects (with their own build system) merge their libraries and binaries into the parent build. Each subproject's build system is inferred from its directory contents — `build.cpp` → cppup, `CMakeLists.txt` → CMake, `Makefile`/`GNUmakefile` → Make, headers only → header-only — and can be overridden explicitly:

```cpp
config.subprojects = {
    Subproject{.path = "src/core/configuration"},
    Subproject{.path = "src/core/dependency"},
    Subproject{.path = "src/core/cli"},
};
```

## Plugins

cppup's runtime extension points (build systems, package sources, loggers) sit behind a plain-C ABI in [include/cppup/plugin/abi.h](include/cppup/plugin/abi.h). Plugins are shared objects with two exported symbols (`cppup_plugin_entries`, `cppup_plugin_manifest`) and a sidecar TOML manifest validated before any plugin code runs. Built-in functionality (e.g. the gtest test framework, the cppup-native build system) is registered through the same path via an in-process `StaticPluginRegistry`.

```bash
cppup plugin list                  # external + builtin entries
cppup plugin add <source>          # currently a stub — full validation pipeline still landing
cppup plugin remove <name>
```

The full spec — ABI, manifest schema, host services, validation rules — lives in [docs/plugin_api.md](docs/plugin_api.md). Note that `plugin add` is presently scaffolding only; the validated install path described in the spec is in progress. For how a plugin distributed as *source* (rather than a pre-built shared object) is meant to be fetched, compiled, and loaded — and which of those steps are actually wired up today — see [docs/plugin_api.md §6.6](docs/plugin_api.md#66-fetching-and-compiling-a-plugin-from-source).

## CLI Commands

```
cppup init <name>                    Scaffold a new project.
    --full                              Include all optional templates
    --minimal                           Base layout only, no prompts
    --with-vscode / --with-devcontainer / --with-docker / --with-gitlab-ci
        / --with-github-actions
                                        Per-template opt-in (skips the TTY prompt)
    --path <dir>                        Create at <dir> instead of <name>/

cppup build                          Build libraries and binaries.
    --asan                              Enable AddressSanitizer
    --coverage                          Instrument with gcov coverage flags
    --with-tests                        Also compile test binaries
    --toolchain <name>                  Override toolchain selection for this run
    --profile   <name>                  Override profile selection for this run
    -V, --verbose                       Echo compile/link commands as they run
    -j, --jobs <N>                      Parallel compile jobs (0 = auto)

cppup test [filter]                  Run tests (compiles them if needed).
    --asan, --coverage, --toolchain, --profile
    [filter]                            Pass-through filter handed verbatim to each test's
                                        TestFramework plugin (gtest: glob like 'Suite.*').
                                        Tests without a configured framework are skipped
                                        when a filter is supplied.

cppup compile-commands               Emit compile_commands.json for clangd/tooling.
    --asan, --coverage                  Mirror those flags in the emitted commands

cppup clean                          Remove build artifacts.
    --all                               Also wipe .cppup/packages, toolchains, plugins, bin

cppup format [files...]              Format with clang-format.
    --check                             Verify formatting without modifying files

cppup tidy [files...]                Run clang-tidy (needs compile_commands.json).
    --fix                               Apply suggested fixes in place

cppup lock                           Alias of `cppup package lock`.
cppup sync                           Alias of `cppup package sync`.

cppup add <ref>                      Short form: install a package by ref.
    <ref>                                git URL, github:/gitlab: shorthand,
                                         directory path, http URL, or bare name
    --name <override>                    Override the inferred name
    --build-system <cppup|cmake|make|header-only>
    --subdirectory <path>                Subpath within fetched repo
    -u, --user                           Install into user data dir

cppup package add <opts>             Install a package.
    --name <name>                       Required
    --git <url> [--branch|--tag|--commit]
    --dir <path>                        Local directory source
    --url <url> | --tar <url> | --zip <url>
                                        Archive sources (placeholder fetch today)
    --version <v>                       Registry source (stub today)
    --subdirectory <path>               Subpath within fetched repo/archive to use
    --build-system <cppup|cmake|make|header-only>
                                        Override inferred backend
    -u, --user                          Install into $XDG_DATA_HOME/cppup (else $HOME/.cppup)
cppup package list                   List installed packages (project + user, tagged).
cppup package remove <name>          Remove a package (searches both scopes).
cppup package lock                   Regenerate cppup.lock from build.cpp.
cppup package sync                   Materialize .cppup/packages/ from cppup.lock.
    -j, --jobs <N>                      Parallel fetches (0 = auto / hardware_concurrency)
    -V, --verbose                       Stream the underlying fetch tool's output

cppup toolchain add <opts>           Install a toolchain.
    -u, --user                          User-scope install
cppup toolchain list                 List toolchains (project + user, tagged).
cppup toolchain remove <name>        Remove a toolchain.
cppup toolchain select <name>        Persist active toolchain to cppup.lock.

cppup profile select <name>          Persist active profile to cppup.lock.

cppup registry set <location>        Persist active registry (URL or path) to cppup.lock.

cppup plugin add <source>            Install a plugin (scaffolding today, full pipeline in progress).
cppup plugin list                    List installed and builtin plugin entries.
cppup plugin remove <name>           Remove a plugin.

cppup module add <name>              Create a new module under the current project.

cppup update                         Install the latest released cppup binary.
    --check                             Print running + latest version, no install
    --version <tag>                     Install a specific tag
    --install-dir <path>                Override install dir (default: $HOME/.cppup/bin)

cppup version                        Print cppup version.
cppup --version                      Same.
```

## Environment Variables

User-set:

- `CXX`, `CC` — compiler probe during `./bootstrap.sh` and a fallback in toolchain selection. Default `g++`.
- `CPPUP_RELEASE_REPO` — override the GitHub `owner/repo` that `cppup update` and the `cppup sync` version-check pull from.
- `CPPUP_NO_VERSION_CHECK` — when set (any non-empty value), suppress the best-effort "new version available" hint that `cppup sync` prints.
- `XDG_DATA_HOME` — when set and non-empty, `--user` installs land at `$XDG_DATA_HOME/cppup/`. Otherwise cppup falls back to `$HOME/.cppup/`.
- `HOME` — fallback root for user-scope installs.
- `CPPUP_SKIP_HOOKS=1` — skip the entire pre-commit hook for one commit.
- `CPPUP_SKIP_GITLEAKS=1` / `CPPUP_SKIP_FORMAT=1` / `CPPUP_SKIP_TIDY=1` / `CPPUP_SKIP_BUILD=1` — skip the individual mandatory checks in `.githooks/pre-commit`.
- `CPPUP_CHECK_COVERAGE=1` — opt in to the pre-commit coverage-threshold gate (off by default: it rebuilds the test suite with gcov instrumentation and reruns everything, via [scripts/check_coverage.sh](scripts/check_coverage.sh)).
- `CPPUP_SKIP_COVERAGE=1` — skip the coverage gate for one commit even if `CPPUP_CHECK_COVERAGE=1` is set globally.
- `CPPUP_MIN_COVERAGE=N` — minimum total line-coverage percentage the gate requires (default: `70`).

Set by the CLI before compiling and loading `build.cpp` — read these via `active_toolchain()` / `active_profile()` or the `when_*` helpers in `<cppup/configuration.hpp>`:

- `CPPUP_ACTIVE_TOOLCHAIN` — always set during `cppup build` / `compile-commands` / `test`.
- `CPPUP_ACTIVE_PROFILE` — set only when a profile is active; explicitly unset otherwise.

## Troubleshooting

Compiler not found or too old:

```bash
export CXX=clang++
./bootstrap.sh
```

cppup's slim bootstrap probes for C++23 and compiles with C++26. Use GCC 13+ or a recent Clang.

`cppup tidy` fails with "compile_commands.json missing":

```bash
cppup compile-commands       # or run a full `cppup build` first
```

A clean checkout that won't build:

```bash
cppup sync                   # materialize packages from cppup.lock
cppup build
```

## Development

- Source layout: `src/core/{configuration,dependency,build,buildsystems,package,cli,logger,plugin,test_frameworks}/`. Each module is consumed as a subproject from the top-level [build.cpp](build.cpp).
- Unit tests live next to their modules under `src/core/**/tests/` (and as `test_*.cpp` siblings in `src/core/plugin/` and friends) and are compiled when you pass `--with-tests` or run `cppup test`.
- Example projects: [examples/simple_project/](examples/simple_project/) and [test_build_project/](test_build_project/).
- Pre-commit hooks (`.githooks/pre-commit`): gitleaks secret scan, `cppup format --check`, `cppup tidy`, plus an opt-in coverage-threshold gate ([scripts/check_coverage.sh](scripts/check_coverage.sh), enabled via `CPPUP_CHECK_COVERAGE=1`) — wired up automatically by `bootstrap.sh` via [scripts/setup-hooks.sh](scripts/setup-hooks.sh). Test the gate's parsing/threshold logic directly with [scripts/tests/test_check_coverage.sh](scripts/tests/test_check_coverage.sh).
- Generated headers: [scripts/embed_init_templates.sh](scripts/embed_init_templates.sh) bakes `templates/init/**` into `init_templates_data.hpp`, and [scripts/amalgamate_configuration_header.sh](scripts/amalgamate_configuration_header.sh) produces the single-header `cppup/configuration.hpp` that user projects `#embed`. Both are re-run by [build.cpp](build.cpp) on every configure.

## Additional References

- Lockfile, package layers, install scope: [docs/packages.md](docs/packages.md)
- Plugin ABI specification: [docs/plugin_api.md](docs/plugin_api.md)
- Configuration API: [src/core/configuration/README.md](src/core/configuration/README.md)
- CLI command implementation: [src/core/cli/README.md](src/core/cli/README.md), [src/core/cli/commands/README.md](src/core/cli/commands/README.md)
- Package backends: [src/core/package/README.md](src/core/package/README.md), [src/core/dependency/README.md](src/core/dependency/README.md)
- Manifest design (`package.toml`): [manifests/README.md](manifests/README.md)

## License

MIT, with a non-binding Beerware addendum — see [LICENSE](LICENSE). If we meet some day and you think this is worth it, you can buy the author a beer.
