# cppup CLI

This directory contains the command-line entry point of cppup: argument
parsing, command dispatch, and the orchestration glue between commands
and the rest of the project (configuration, package, plugin, build).
For the user-facing command list and flag reference, see the
top-level [README.md](../../../README.md#cli-commands).

## Layout

```
src/core/cli/
├── cli_application.{hpp,cpp}    # argument parser, command registration
├── commands.{hpp,cpp}           # command interface types and option structs
├── command_context.hpp          # shared context (logger, project root, env)
├── commands/                    # one file per command group (see below)
└── CLI/                         # vendored CLI11 parser
```

The CLI binary is the same source tree as the rest of cppup — it links
against `src/core/{configuration,dependency,package,plugin,buildsystems,…}`
and consumes them as ordinary in-process modules. The "slim" bootstrap
binary used during `bootstrap.sh` is a stripped subset that only
includes the `build` and `update` commands; see
[`cppup_bootstrap.cpp`](../../../cppup_bootstrap.cpp).

## Command groups

Each group is a separate file in [commands/](commands/) and registers
through `CliApplication::register*Commands`:

| Group | File | Subcommands |
|---|---|---|
| Project | [init.cpp](commands/init.cpp) | `init` |
| Build | [build.cpp](commands/build.cpp), [clean.cpp](commands/clean.cpp), [compile_commands_cmd.cpp](commands/compile_commands_cmd.cpp) | `build`, `clean`, `compile-commands` |
| Test | [test.cpp](commands/test.cpp) | `test` |
| Code style | [format.cpp](commands/format.cpp), [tidy.cpp](commands/tidy.cpp) | `format`, `tidy` |
| Packages | [package.cpp](commands/package.cpp) | `package add \| list \| remove \| lock \| sync` and top-level aliases `lock`, `sync` |
| Toolchains | [toolchain.cpp](commands/toolchain.cpp) | `toolchain add \| list \| remove \| select`, `profile select` (shares the lockfile selection machinery) |
| Registry | [registry.cpp](commands/registry.cpp) | `registry set` |
| Modules | [module.cpp](commands/module.cpp) | `module add` |
| Plugins | [plugin.cpp](commands/plugin.cpp) | `plugin add \| list \| remove` (`add` is scaffolding only — see [docs/plugin_api.md §6](../../../docs/plugin_api.md)) |
| Plugin commands | [plugin_cli_commands.cpp](commands/plugin_cli_commands.cpp) | one `cppup <name>` subcommand per `CPPUP_KIND_CLI_COMMAND` plugin, registered after the built-ins so a plugin can't shadow a core command — see [docs/plugin_api.md §7.4](../../../docs/plugin_api.md) |
| Self-update | [update.cpp](commands/update.cpp) | `update` |
| Version | inline in `cli_application.cpp` | `version`, `--version` |

Shared helpers — file discovery, command execution, archive
extraction, install-path resolution — live in
[commands/common.h](commands/common.h) and
[commands/install_paths.cpp](commands/install_paths.cpp). The lockfile
parser/writer used by `lock`/`sync`/`*-select` is in
[commands/lockfile.cpp](commands/lockfile.cpp).

## How a typical command runs

1. CLI11 parses argv into the strongly-typed `*Options` structs declared
   in [commands.hpp](commands.hpp).
2. `cli_application.cpp` constructs a `CommandContext` (project root,
   logger, env) and dispatches to the relevant `execute*` function.
3. The command returns `std::expected<int, std::string>`; the CLI
   prints any error to stderr and propagates the exit code.
4. For commands that need the project's `BuildConfiguration`
   (`build`, `test`, `compile-commands`, `lock`), `executeBuild` and
   friends call `load_project_configuration()` to compile `build.cpp`,
   `dlopen` the result, and call `configure()` — see the build pipeline
   below.

## Build pipeline (after `configure()` returns)

When you run `cppup build`, the order of operations is:

1. **Auto-sync.** If `cppup.lock` exists, run the equivalent of
   `cppup package sync` to materialize `.cppup/packages/` from the
   lockfile.
2. **Legacy migration.** Fold any `.cppup/toolchain.txt` into
   `cppup.lock`'s `selected_toolchain` and delete the legacy file.
3. **Early selection.** Resolve toolchain + profile from CLI flags >
   lockfile > env (`$CXX`/`$CC`) > defaults (see
   [selection_resolver.cpp](commands/selection_resolver.cpp)). Export
   `CPPUP_ACTIVE_TOOLCHAIN` / `CPPUP_ACTIVE_PROFILE` so `build.cpp`'s
   `when_*` helpers can branch on them.
4. **Compile & load `build.cpp`.** Compile against the amalgamated
   `<cppup/configuration.hpp>`, `dlopen` the resulting `.so`, call
   `configure()`.
5. **Final selection.** Re-resolve with `config.toolchain` slotted
   between lockfile and env, so `build.cpp`'s own default participates.
6. **Toolchain expansion.** Map `Toolchain::cxx_standard` /
   `Toolchain::warnings` to family-specific flags
   ([toolchain_flags.cpp](../configuration/toolchain_flags.cpp)).
7. **Profile expansion.** Apply the active profile's flags / definitions
   onto the global lists ([profile_processor.cpp](../configuration/profile_processor.cpp)).
8. **Build-step execution.** Run any `BuildStep` entries in dependency
   order.
9. **Compile + link.** Build libraries, binaries, and (if `--with-tests`
   or running under `cppup test`) test binaries in parallel
   (`-j N`, default = `std::thread::hardware_concurrency`).
10. **Test run** (under `cppup test` only). Each test binary is
    discovered via its `TestFramework`'s `list_test_cases` and invoked
    via `run`.

`cppup compile-commands` runs steps 3–7 and emits
`compile_commands.json`; it does not compile or link.

## Error handling

All commands return `std::expected<int, std::string>`. The pattern at
each call site is:

```cpp
auto result = executeFoo(opts, ctx);
if (!result) return std::unexpected{result.error()};
```

Internal invariant violations (corrupted in-memory registries, etc.)
use `CPPUP_CHECK` and panic — they are not user-facing errors. Any
error a user can plausibly cause (bad input, missing file, network
failure) must be returned as `std::unexpected`.

## Tests

Command-level tests live next to their implementations in
[commands/](commands/) as `test_*.cpp` files (e.g.
`test_package_command.cpp`,
`test_lockfile.cpp`). They are picked up by the project-wide test
target.
