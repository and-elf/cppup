# Package sources

This directory implements the package-fetching side of cppup: given a
`PackageInfo` (which source kind, where to pull it from), it produces
a local directory under `.cppup/packages/<name>/` ready to be picked up
by a build-system backend.

For the user-facing model (lockfile, sync, install scope), see
[docs/packages.md](../../../docs/packages.md). For the plugin ABI that
package sources implement, see
[docs/plugin_api.md §3.4](../../../docs/plugin_api.md).

## Layout

```
src/core/package/
├── package_concept.h/cpp     # PackageType concept + PackageCacheInterface
├── package_factory.h/cpp     # dispatch PackageInfo -> concrete source
├── packages.h                # umbrella header
├── git/                      # GitPackage      — git clone + checkout
├── directory/                # DirectoryPackage — local path passthrough
├── archive/                  # ArchivePackage   — tar.gz / zip (placeholder fetch)
├── http/                     # HttpPackage      — single file / archive (placeholder fetch)
└── registry/                 # RegistryPackage  — stub
```

## Source kinds and current behaviour

| Kind | Source dir | CLI form | Behaviour today |
|---|---|---|---|
| `git`       | `git/`       | `--git URL [--branch B \| --tag T \| --commit C]` | Working: clones, checks out, caches by URL+ref. |
| `directory` | `directory/` | `--dir PATH`                                    | Working: uses the path in place (no copy). |
| `tar`       | `archive/`   | `--tar URL`                                     | Stub: creates an empty `.cppup/packages/<name>/`. |
| `zip`       | `archive/`   | `--zip URL`                                     | Stub: creates an empty `.cppup/packages/<name>/`. |
| `http`      | `http/`      | `--url URL`                                     | Stub: creates an empty `.cppup/packages/<name>/`. |
| `registry`  | `registry/`  | `--name N --version V`                          | Stub: `resolve_source()` returns an error. |

The stubs exist so the lockfile schema and on-disk layout can stabilize
ahead of the actual fetch implementations. The deferred work is tracked
in [docs/packages.md](../../../docs/packages.md) under "Out of scope".

## Concept

Each source type satisfies `cppup::package::PackageType`:

```cpp
template <typename T>
concept PackageType = requires(T t) {
  { t.info() }            -> std::convertible_to<const PackageInfo&>;
  { t.resolve_source() }  -> std::convertible_to<std::expected<std::filesystem::path, std::string>>;
  { t.set_command_executor(std::shared_ptr<CommandExecutor>{}) } -> std::same_as<void>;
};
```

`PackageFactory::create_package` dispatches on `info.source_type` and
returns a type-erased handle. Concrete types are also constructible
directly (`cppup::package::git::GitPackage{info}`) when callers want to
avoid the dispatch.

## Host services injected into sources

- `CommandExecutor` — wraps process invocation (`git clone`, archive
  extraction, etc.) so tests can swap in a fake. Set via
  `set_command_executor`.
- `PackageCacheInterface` — abstract cache for resolved content. The
  default implementation keys by package name under `.cppup/packages/`
  and is shared across source types.

Both surfaces also exist as plain-C structs in the plugin ABI
(`cppup_cmd_exec_v1`, `cppup_cache_v1`) so out-of-tree package-source
plugins get the same view.

## Usage

```cpp
#include "src/core/package/packages.h"

auto info = PackageInfo{
    .name        = "fmt",
    .source_type = SourceType::Git,
    .url         = "https://github.com/fmtlib/fmt.git",
    .git_branch  = "10.2.1",
};

auto package  = cppup::package::make_package(std::move(info));
package.set_command_executor(std::make_shared<MyCommandExecutor>());

auto source_path = package.resolve_source();
if (!source_path) {
  std::cerr << source_path.error() << "\n";
}
```

The CLI calls this from `cppup package add` (immediate fetch) and from
`cppup package sync` (lockfile → disk).

## How packages reach the build

`cppup sync` materializes `.cppup/packages/<name>/` but **does not** by
itself add include paths or link flags to the project. Today, consumers
add what they need explicitly in `build.cpp`:

```cpp
config.include_paths.push_back(".cppup/packages/fmt/include");
```

Automatic library export — using `[exports]` from a `package.toml`
manifest to drive include paths and link flags — is the next major
piece of work, tracked in [../../../manifests/README.md](../../../manifests/README.md).
The schema is designed; the wiring through `cppup build` is not.

## Tests

Unit tests for each source live alongside the implementation (e.g.
`git/test_git_package.cpp`) and use `FakeCommandExecutor` /
`InMemoryPackageCache` from `package_concept.cpp` to avoid hitting the
network or disk.
