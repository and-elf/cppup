# cppup Plugin API — Specification v1

Status: **Partial implementation in tree.** The ABI in
[include/cppup/plugin/abi.h](../include/cppup/plugin/abi.h), the manifest
parser, descriptor validator, dynamic loader, and static-plugin registry
are landed. `cppup plugin add` is still a scaffolding stub — the
validation pipeline described in §6 has not been wired through. Built-in
extensions (cppup-native build system, console logger, gtest test
framework) register through the same plugin pipeline rather than ad-hoc
factories.

This document is the source of truth for the plugin system. The ABI
header is the ultimate reference for byte-level layout; this document
explains *why* the ABI looks the way it does and how the host uses it.
If the spec and the code disagree, file an issue — both sides should be
moved into alignment, not allowed to drift.

## 1. Goals & non-goals

### 1.1 Goals
- Allow third-party shared objects to extend cppup along three runtime axes
  without re-linking: **build systems**, **package sources**, **loggers**.
- One shared object may contribute multiple plugin entries (e.g. a single
  `cppup-build-systems.so` providing both `ninja` and `meson`).
- Every plugin self-describes via a TOML manifest stamped at the plugin's
  own build time. The host validates the manifest before any code runs.
- Plugin install/remove is **explicit**, driven by `cppup plugin add /
  remove / list`. No auto-discovery from arbitrary filesystem locations.
- Compile-time extension (helper code injected into the user's
  `configure()`) happens via plain headers shipped by the plugin, called
  explicitly by the user — not as an automatic plugin kind.

### 1.2 Non-goals (v1)
- Out-of-process / sandboxed plugins. The architecture leaves room for
  this (see §13) but v1 loads plugins in-process under full trust.
- Cross-version binary compatibility across major cppup releases. A
  plugin stamped for `0.2.x` is not expected to load under `0.3.0`.
- A package format / signing infrastructure. The manifest carries hashes
  for tamper detection; trust establishment is the user's problem.
- Automatic resolution of plugin dependencies on other cppup plugins.
  The manifest's `[plugin.dependencies].plugins` is informational in v1.

## 2. Plugin kinds

Three runtime kinds today, two reserved for post-v1. Each runtime kind
maps to an existing cppup concept — the plugin layer does not invent
new abstractions; it lets external code satisfy existing ones via a
stable C ABI.

| Kind enum                    | Existing concept                                 | Lives in                                                       | Status |
| ---                          | ---                                              | ---                                                            | --- |
| `CPPUP_KIND_BUILD_SYSTEM = 1`    | `cppup::buildsystems::*Package`              | [src/core/buildsystems/](../src/core/buildsystems/)            | v1 vtable frozen |
| `CPPUP_KIND_PACKAGE_SOURCE = 2`  | `cppup::package::PackageType`                | [src/core/package/](../src/core/package/)                      | v1 vtable frozen |
| `CPPUP_KIND_LOGGER = 3`          | `cppup::logger::LoggerType`                  | [src/core/logger/](../src/core/logger/)                        | v1 vtable frozen |
| `CPPUP_KIND_TEMPLATE = 4`        | `cppup init --type <name>` scaffolders       | n/a                                                            | reserved, no vtable yet |
| `CPPUP_KIND_TEST_SYSTEM = 5`     | `cppup::TestFrameworkPlugin`                 | [src/core/test_frameworks/](../src/core/test_frameworks/)      | reserved; today exposed only via the in-process C++ registry (§11) |

A "config-hook" kind is **not** part of the plugin ABI. A plugin that
needs to augment a user's `BuildConfiguration` ships a header (e.g.
`include/foo_plugin.hpp`) with regular C++ functions; the user
`#include`s it and calls it explicitly from `build.cpp`. Plugins never
mutate configuration implicitly.

## 3. ABI

The shared-object boundary is **plain C**. C++ ABI is too fragile
across compilers and stdlibs to use across `dlopen`. cppup's internal
C++ types adapt the C vtables on the host side.

The ABI lives in a single installed header,
`include/cppup/plugin/abi.h`, copied into the plugin's source tree by
`cppup init --plugin`.

### 3.1 Entry points

Exactly two symbols are exported per shared object:

```c
// Returns the list of plugin entries this SO provides.
// The returned pointer and the descriptors it points to have static
// storage lifetime; the host does not free them.
const cppup_plugin_descriptor* const*
cppup_plugin_entries(size_t* out_count);

// Returns the manifest TOML embedded at the plugin's build time.
// Lifetime: static. Never NULL.
const char* cppup_plugin_manifest(void);
```

Anything else in the SO is private to the plugin.

### 3.2 Descriptor

```c
typedef enum {
  CPPUP_KIND_BUILD_SYSTEM    = 1,
  CPPUP_KIND_PACKAGE_SOURCE  = 2,
  CPPUP_KIND_LOGGER          = 3,
} cppup_plugin_kind;

typedef struct {
  const char*       id;              // unique within the SO; matches manifest entry id
  cppup_plugin_kind kind;
  uint32_t          vtable_version;  // identifies the layout of *vtable
  const void*       vtable;          // points at one of cppup_*_vtable_vN below
} cppup_plugin_descriptor;
```

### 3.3 Per-vtable versioning

There is **no** global `CPPUP_PLUGIN_ABI_VERSION`. Each vtable type has
its own integer version. Bumping one extension axis (e.g. logger) does
not invalidate plugins that only contribute another axis (e.g. package
sources). Initial versions for v1:

| Type                              | Initial version |
| ---                               | ---             |
| `cppup_build_system_vtable_v1`    | 1               |
| `cppup_package_source_vtable_v1`  | 1               |
| `cppup_logger_vtable_v1`          | 1               |

A descriptor's `vtable_version` must equal the version of the struct
its `vtable` field points at. The host rejects any descriptor whose
`(kind, vtable_version)` pair it does not recognise.

### 3.4 Vtables

[`include/cppup/plugin/abi.h`](../include/cppup/plugin/abi.h) is the
authoritative reference. The summary below highlights the design points
that matter when writing or porting a plugin; the header has the field
list and per-field documentation.

**Status codes** (`cppup_status`): `CPPUP_OK = 0`, `CPPUP_ERR_GENERIC = 1`,
`CPPUP_ERR_NOT_SUPPORTED = 2`, `CPPUP_ERR_IO = 3`, `CPPUP_ERR_INVALID_ARG = 4`,
`CPPUP_ERR_BUFFER_TOO_SMALL = 5`. Append-only — never renumber.

**Source types** (`cppup_source_type`): `DIRECTORY = 0`, `GIT = 1`,
`TAR = 2`, `ZIP = 3`, `HTTP = 4`, `REGISTRY = 5`. Append-only. Mirrors
`cppup::configuration::SourceType`.

**Package info struct** (`cppup_package_info_v1`): name (required, never
NULL) plus optional `version`, `source_directory`, `url`, `source_type`,
`git_branch`, `git_commit`, `subdirectory`, and a NULL-terminated
`build_args` array. **Optional fields are `NULL` when absent, not empty
strings.** The struct lifetime is the call; plugins copy fields they
need to retain.

**Logger vtable v1** — pure sink:
- `create(const char* config_toml)` → opaque instance
- `destroy(instance)`
- `log(instance, level, message, len)` — `message` is UTF-8 of length
  `len`, **not** NUL-terminated. Levels match
  `cppup::logger::LogLevel` (`Debug=0`, `Info=1`, `Warning=2`,
  `Error=3`).
- `last_error(instance)` — owned by the plugin, valid until the next
  call on the same instance.

**Package source vtable v1** — handles exactly one
`accepted_type`. The host dispatches by `cppup_source_type`:
- `create(info)` / `destroy(instance)`
- `resolve_source(instance, out, cap, out_needed)` — **two-call
  pattern**: pass `cap=0` (and `out=NULL`) first to read `*out_needed`,
  then pass a sized buffer. Returns `CPPUP_ERR_BUFFER_TOO_SMALL` when
  `out_needed` is set.
- `set_command_executor(instance, cppup_cmd_exec_v1*)` and
  `set_cache(instance, cppup_cache_v1*)` — see §3.5. Called at most
  once per instance and before any `resolve_source`. NULL detaches.
- `last_error(instance)`.

**Build-system vtable v1** — uses the **visitor pattern** for flag
accessors instead of caller-allocated buffers. The plugin calls
`visit(user, str, len)` once per flag / path, in declaration order:
- `create(info)` / `destroy(instance)`
- `build(instance, source_path)` → `cppup_status`
- `get_compile_flags(instance, visit, user)` — and likewise
  `get_link_flags`, `get_include_paths`, `get_library_paths`. Strings
  passed to `visit` are valid only for the duration of the call.
- `set_command_executor(instance, cppup_cmd_exec_v1*)` — build systems
  do not get a cache pointer; source resolution is already a
  package-source concern.
- `last_error(instance)`.

The build-system flag accessors deliberately differ from the
package-source resolver: flags are inherently a sequence, so the
visitor avoids the two-call sizing dance. Single-shot path results
(`resolve_source`) use two-call sizing because the natural caller
wants a single `std::filesystem::path` and copying via a visitor would
just complicate the host adapter.

### 3.5 Host services

The host injects services into plugin instances via the `set_*`
functions on each vtable. These structs are owned by the host and live
strictly longer than any plugin instance that holds them.

**`cppup_cmd_exec_v1`** — process execution for package sources and
build systems (clones, downloads, build invocations):

- `execute(state, command, working_dir)` → status
- `execute_with_output(state, command, working_dir, visit, user)` —
  delivers captured stdout to `visit` in chunks (NUL-terminated UTF-8
  of length `len`). If `visit` is NULL, output is discarded. Two-call
  sizing is intentionally **not** used here: process invocation is
  not idempotent.
- `last_error(state)` — host-owned, valid until the next call.

**`cppup_cache_v1`** — package cache service for package-source
plugins:

- `get_cache_directory(state, out, cap, out_needed)` — root of the
  cache (two-call sizing).
- `get_package_cache_path(state, info, out, cap, out_needed)` — the
  directory this specific package should materialize into.
- `is_cached(state, info)` → `0/1`
- `clear_package_cache(state, info)` and `clear_all_cache(state)`.

A plugin that calls into these services through the host adapter (see
`src/core/plugin/host_service_adapters.{hpp,cpp}`) gets a uniform view
regardless of whether it ended up in-process via the static registry
or via `dlopen`.

### 3.6 String and buffer rules

- All strings crossing the ABI are NUL-terminated UTF-8, except where
  explicitly paired with a length (`log`, the `visit` callback strings).
- Out-buffers follow the **two-call pattern** where used: pass `cap=0`
  (and `out=NULL`) to query required size via `out_needed`, then pass a
  sufficient buffer to receive data. If `cap` is non-zero but too
  small, the function writes nothing, sets `out_needed`, and returns
  `CPPUP_ERR_BUFFER_TOO_SMALL`.
- Pointers passed by the host into the plugin are valid only for the
  duration of the call (including `cppup_package_info_v1*` and any
  strings inside it).
- Pointers returned by the plugin must remain valid at least until
  the next call into the same instance or the plugin SO, whichever
  comes first (i.e. plugins own their return buffers).
- `VtableSupport` (in `src/core/plugin/vtable_support.{hpp,cpp}`) is
  the host-side table of `(kind, vtable_version)` pairs the running
  cppup understands. The descriptor validator rejects anything not in
  this set with `VtableVersionUnsupported`; this is how forward-incompat
  changes are detected without a global ABI version bump.

## 4. Manifest TOML

The manifest sidecar is installed **next to the shared object and
named after it**: append `.toml` to the full SO filename.

| SO filename             | Sidecar filename               |
| ---                     | ---                            |
| `libcppup_ninja.so`     | `libcppup_ninja.so.toml`       |
| `libcppup_ninja.dylib`  | `libcppup_ninja.dylib.toml`    |
| `cppup_ninja.dll`       | `cppup_ninja.dll.toml`         |

Given any SO path, the sidecar path is `so_path + ".toml"`. This
removes the need to assume a single SO per directory and makes
ownership unambiguous when scanning. The same TOML is also embedded
as a string symbol inside the SO (`cppup_plugin_manifest()`) for
tamper detection.

### 4.1 Schema (schema = 1)

```toml
schema = 1

[plugin]
name          = "cppup-ninja"           # required; ^[a-z][a-z0-9_-]*$
version       = "0.3.1"                 # required; semver
cppup_compat  = ">=0.2.0,<0.3.0"        # required; semver range
build_hash    = "sha256:9f1a...e1"      # required; lowercase hex of the .so contents
commit_hash   = "a3f1c2d"               # required; arbitrary string
build_date    = "2026-05-21T10:14:00Z"  # required; RFC 3339
license       = "MIT"                   # required; SPDX or "proprietary"
homepage      = "https://..."           # optional

[[plugin.entries]]                       # required; >= 1 entry
id              = "ninja"               # required; ^[a-z][a-z0-9_-]*$; unique within the SO
kind            = "build_system"        # required; one of "build_system" | "package_source" | "logger"
vtable_version  = 1                     # required; matches the descriptor returned at runtime
description     = "Ninja build system support"  # optional

[[plugin.entries]]
id              = "meson"
kind            = "build_system"
vtable_version  = 1

[plugin.dependencies]                    # optional table
system  = ["libninja-build1 >= 1.11"]    # informational; not enforced by cppup
plugins = []                             # names of other cppup plugins; v1: informational
```

### 4.2 Validation rules

The host parses with **toml++**. Any of the following → reject:

1. `schema != 1`.
2. Missing required field, or wrong scalar type.
3. `plugin.name` or any `entries[].id` fails the regex.
4. Duplicate `entries[].id` within the file.
5. `version` or `cppup_compat` not a valid semver string / range.
6. `build_hash` is not exactly `sha256:` followed by 64 lowercase hex chars.
7. `build_date` is not parseable as RFC 3339.
8. `entries[].kind` is not in the known set.
9. The sidecar manifest and the embedded `cppup_plugin_manifest()` string
   differ in the **entries set** (ids and kinds). The non-entries fields
   are allowed to drift only between sidecar and embedded for fields the
   user controls outside the SO — see §6.4.

## 5. Build-time stamping (plugin author side)

A plugin author maintains a hand-edited `plugin.toml` with the static
fields (name, version, license, entries, dependencies). The dynamic
fields (`build_hash`, `commit_hash`, `build_date`, `cppup_compat`) are
filled in by a generator the SDK provides:

```
cppup-plugin-stamp \
    --in  plugin.toml \
    --so  build/libcppup_ninja.so \
    --cppup-version 0.2.0 \
    --out build/libcppup_ninja.so.toml \
    --emit-embedded build/cppup_plugin_manifest.cpp
```

`--out` defaults to `<so-path>.toml` when omitted.

`cppup_plugin_manifest.cpp` is a generated TU defining the
`cppup_plugin_manifest()` function as a `const char[]` over the same
TOML. It is linked into the plugin SO so the manifest travels with the
binary.

`cppup-plugin-stamp` is a separate binary (under `src/core/cli/` as a
secondary entry point, or as a standalone tool — TBD by implementation).

## 6. CLI: `cppup plugin add | remove | list`

The validated install pipeline below is the target design. The current
implementation in
[src/core/cli/commands/plugin.cpp](../src/core/cli/commands/plugin.cpp)
is a thin stub: `plugin add` creates `.cppup/plugins/<name>/` with a
`manifest.json` placeholder, `plugin list` walks the directory plus the
static registry, and `plugin remove` deletes the directory. None of the
manifest parsing, hash verification, `dlopen` validation, or
`installed.toml` registry described below is wired through yet. The
manifest parser and descriptor validator that this pipeline will call
into are in `src/core/plugin/{manifest,descriptor_validation}.cpp` and
fully tested in isolation.

### 6.1 On-disk layout

```
<project_root>/.cppup/
└── plugins/
    ├── installed.toml                   # ordered registry; see §6.5
    ├── cppup-ninja/
    │   ├── libcppup_ninja.so
    │   └── libcppup_ninja.so.toml
    └── cppup-pip/
        ├── libcppup_pip.so
        └── libcppup_pip.so.toml
```

The directory name under `plugins/` is always the manifest's
`plugin.name` — not a user choice.

### 6.2 `cppup plugin add <source>`

`<source>` accepts the same forms the current command takes: local
directory, URL, archive, git ref. Existing options (`--version`,
`--tag`, `--url`, `--dir`) carry over.

Steps:

1. Resolve the source into a temp staging dir.
2. Locate exactly one SO (`*.so` on Linux, `*.dylib` on macOS,
   `*.dll` on Windows). Its sidecar must be at `<so-path>.toml`.
   Reject if the SO is missing, the sidecar is missing, or more than
   one SO is present.
3. Parse the manifest (§4). Reject on any validation failure.
4. `dlopen` the SO with `RTLD_LAZY | RTLD_LOCAL`.
5. `dlsym` `cppup_plugin_entries` and `cppup_plugin_manifest`. Both
   missing → reject.
6. Read `cppup_plugin_manifest()`, parse it, and compare the **entries
   set** to the sidecar. Disagreement → reject.
7. For each descriptor returned by `cppup_plugin_entries`:
   - `(kind, vtable_version)` must be in the host's known set.
   - `id` must appear in the sidecar's `[[plugin.entries]]`.
8. Compute SHA-256 of the SO file. Compare against
   `plugin.build_hash`. Mismatch policy:
   - Release builds of cppup: **reject**.
   - Debug builds (`CMAKE_BUILD_TYPE=Debug`): **warn + accept** to
     support the plugin author's edit-test loop where the SO is
     rebuilt without re-stamping. The warning is non-suppressible.
9. Compat check: cppup's running version must satisfy `cppup_compat`.
   Mismatch → reject.
10. `dlclose`. If any prior step failed, leave the temp dir untouched
    so the user can inspect it; print its path.
11. On success, atomically rename the staged directory to
    `.cppup/plugins/<name>/`. If the target exists, reject unless
    `--replace` is passed (in which case the old dir is removed only
    after the new one is fully validated).
12. Append an entry to `.cppup/plugins/installed.toml`.

Return type at the API layer: `std::expected<void, AddError>` where
`AddError` is a strong enum + diagnostic string. This is a user-input
boundary, so `std::expected` is correct per the project's assertion
refactor.

### 6.3 `cppup plugin remove <name>`

1. Reject if `<name>` is not in `installed.toml`.
2. Remove its entry from `installed.toml`.
3. Remove `.cppup/plugins/<name>/`.

Does not affect plugins that depended on the removed one — dependency
resolution is out of scope for v1 (see §1.2). The user is responsible.

### 6.4 `cppup plugin list`

Reads `installed.toml` and the in-process static-plugin registry
(see §7.3). Output for each installed plugin: name, version, source
it was added from (or `builtin` for static-linked entries), install
date, and the list of entries (id + kind) from the on-disk sidecar
manifest. Does not `dlopen` anything. External entries are tagged
`[external]`; static-linked entries are tagged `[builtin]` so the
user can tell them apart.

### 6.5 `installed.toml`

```toml
schema = 1

[[plugin]]
name        = "cppup-ninja"
version     = "0.3.1"
source      = "https://example.com/cppup-ninja-0.3.1.tar.gz"
source_kind = "url"                     # "url" | "git" | "dir" | "archive"
added_at    = "2026-05-21T11:32:18Z"
```

Order in the file is install order. Load order at build time follows
this order; plugins that need to be loaded earlier must be added
earlier. (Topological dependency-aware ordering is a v1.1 feature.)

## 7. Load lifecycle at build time

cppup loads plugins once per `cppup build` invocation, before the
project's `cppup.cpp` is compiled.

1. Read `installed.toml`. If missing or empty: no plugins, continue.
2. For each entry, in `installed.toml` order:
   a. Read the sidecar `cppup_plugin.toml`.
   b. Re-validate (full §4.2 schema check + compat check). Failure
      → warn, skip this plugin, continue with the rest.
   c. `dlopen` the SO. Failure → warn, skip.
   d. `dlsym` the two entry points; verify embedded manifest matches
      sidecar entries set.
   e. For each descriptor:
      - Look up the registry for its kind (build system / package
        source / logger).
      - Wrap the C vtable in a C++ adapter (`PluginBuildSystem`,
        `PluginPackageSource`, `PluginLogger`).
      - Register under `(plugin.name, descriptor.id)`.
   f. Hand the `dlopen` handle to the `PluginHost`.
3. After registration, the existing `PackageFactory` /
   `BuildSystemRegistry` / `LoggerRegistry` consult their plugin maps
   in addition to the built-in static maps.

Validation failure in a single plugin **never** stops the build of
plugins ranked after it; it does fail the overall build only if the
user's `BuildConfiguration` references something only the failed
plugin would have provided.

### 7.1 Ownership & teardown

`PluginHost` owns `dlopen` handles via a rule-of-zero RAII wrapper
(`std::unique_ptr<void, DlCloser>`). Teardown order:

1. Deregister all plugin entries from all registries.
2. Drop the `PluginHost` — `dlclose` runs for every SO.

Any object handed out by a plugin (e.g. a package source instance)
must be destroyed via its vtable's `destroy` before the corresponding
SO is closed. The adapters enforce this by holding a `shared_ptr` to
the `PluginHost`.

### 7.2 Symbol isolation

`RTLD_LOCAL` is mandatory. Plugins must not see each other's symbols.
Plugins should not depend on cppup's internal symbols — they only see
their own copy of the SDK headers and the C ABI.

### 7.3 Static-linked plugins

cppup's own internal extension points — the built-in loggers, package
sources, and build systems — are themselves plugins that ship inside
the cppup binary. They register at process startup through a
`StaticPluginRegistry` rather than via `dlopen`, but they go through
the same manifest parse + descriptor validation pipeline as external
plugins. The only difference: there is no SO file, so the manifest's
`build_hash` field is a sentinel (`sha256:00…00`) and the host skips
file-hash verification for static entries.

Static plugins live alongside dlopen'd ones in the same per-kind
registries used at build time, so user configurations can't tell
which side an entry came from. `cppup plugin list` surfaces both
populations, marking each entry's origin (see §6.4).

## 8. Plugin SDK headers

`cppup init --plugin <name>` scaffolds:

```
<name>/
├── cppup.cpp                       # configure() for the SO itself
├── plugin.toml                     # static manifest fields
├── include/
│   └── <name>_plugin.hpp           # optional: header for users to #include from their cppup.cpp
└── src/
    └── <name>.cpp                  # implements vtables, defines cppup_plugin_entries
```

The SDK installs (next to cppup's own headers):

```
include/cppup/plugin/
├── abi.h                           # the C ABI from §3
└── sdk.hpp                         # C++ helpers
```

### 8.1 `sdk.hpp` shape

```cpp
namespace cppup::plugin::sdk {

template <class Impl>
concept PackageSourceImpl = requires(Impl impl /*, ...*/) {
  // C++ concept that mirrors the C vtable, lets authors write
  // idiomatic C++ instead of populating raw function pointers.
};

template <class Impl> struct PackageSourceAdapter;
// Generates a static cppup_package_source_vtable_v1 from Impl.

#define CPPUP_REGISTER_PLUGIN(...)
// Emits cppup_plugin_entries() given a list of Entry<Impl>{id, ...}.
}
```

The macro expands to a translation unit defining
`cppup_plugin_entries`. It also `static_assert`s that the
`cppup_compat` range in `plugin.toml` is consistent with the
`abi.h` version it was compiled against (the abi.h carries an
informational `CPPUP_PLUGIN_SDK_VERSION` macro).

### 8.2 Header-only "configure-time" extension

A plugin that wants to contribute helpers to the user's `configure()`
ships a header — for example `include/foo_plugin.hpp` — exposing
ordinary C++ functions:

```cpp
// foo_plugin.hpp
namespace foo {
  cppup::config::Package boost_with_modules(std::initializer_list<std::string>);
}
```

The user writes:

```cpp
#include <cppup_config.hpp>
#include <foo_plugin.hpp>

BuildConfiguration configure() {
  return {
    .packages = { foo::boost_with_modules({"system", "filesystem"}) },
    // ...
  };
}
```

No runtime registration, no plugin kind, no implicit invocation.
Plugin authors are free to layer this on top of the runtime ABI or
ship it alone.

## 9. Error semantics & invariants

Per the project's assertion / error-handling policy:

| Layer                                                | Error mechanism                         |
| ---                                                  | ---                                     |
| `cppup plugin add` validation                        | `std::expected<void, AddError>`         |
| Manifest parsing                                     | `std::expected<Manifest, ManifestError>`|
| `dlopen` / `dlsym` failures                          | `std::expected<...>`                    |
| Build-time load loop (per-plugin failure)            | logged warning + skip; no exception     |
| Vtable function failures returned by plugins         | `cppup_status` → `std::expected` at the adapter |
| Host invariants after successful load (e.g. registry corruption) | `CPPUP_CHECK` panic |

No `try/catch` is added around third-party plugin code in v1. A
plugin that throws or crashes takes down the cppup process; an
out-of-process model (§13) addresses this later.

## 10. Test plan (TDD seed)

Each numbered item below is one or more tests, red before any
implementation code goes in.

1. Manifest parser
   - 1.1 Accepts the canonical example from §4.1.
   - 1.2 Rejects each individual validation rule in §4.2 (one test per rule).
   - 1.3 Accepts an entries-only `dependencies` table.
   - 1.4 Reject unknown top-level keys (strict mode).

2. ABI version handling
   - 2.1 Host with `cppup_logger_vtable_v1` known accepts a logger descriptor with `vtable_version = 1`.
   - 2.2 Host rejects descriptor with `vtable_version = 2` for the same kind (forward incompat).
   - 2.3 Bumping logger version does not affect package-source plugin loading (independence).

3. `cppup plugin add`
   - 3.1 Happy path: local dir source, valid SO + sidecar → installed.
   - 3.2 Hash mismatch in release build → reject. In debug → warn + accept.
   - 3.3 Compat range exclusion → reject.
   - 3.4 Embedded vs sidecar entries-set disagreement → reject.
   - 3.5 `--replace` swaps cleanly; without it, duplicate name → reject.
   - 3.6 Staging dir survives a failed add and is reported to the user.

4. `cppup plugin remove`
   - 4.1 Removes the directory and the `installed.toml` entry.
   - 4.2 Rejects unknown name.

5. `cppup plugin list`
   - 5.1 Does not call `dlopen`.
   - 5.2 Output format is stable (golden test).

6. Build-time load
   - 6.1 A failing plugin does not prevent later plugins from loading.
   - 6.2 Registries expose plugin-provided entries alongside built-in ones.
   - 6.3 Teardown destroys all plugin instances before `dlclose`.
   - 6.4 `RTLD_LOCAL` is enforced (symbol leakage test using two plugins with same private symbol name).

7. SDK scaffolding
   - 7.1 `cppup init --plugin foo` produces a tree that builds with stock cppup.
   - 7.2 The produced SO loads via `cppup plugin add ./foo/build/`.

8. Header-only extension
   - 8.1 A user `cppup.cpp` that `#include`s `foo_plugin.hpp` and calls `foo::*` compiles without the runtime plugin installed (proves the header is independent of the SO).

## 11. Test-framework plugins (internal, pre-ABI)

`CPPUP_KIND_TEST_SYSTEM` is reserved in the C ABI but its vtable is
deliberately undefined for v1. Until a stable test-system ABI lands,
cppup exposes test frameworks through an **internal C++ interface**:

- [`src/core/test_frameworks/`](../src/core/test_frameworks/) holds
  `TestFrameworkPlugin` (the abstract base) and concrete framework
  plugins (gtest today).
- `TestFrameworkRegistry` is a process-global registry of pointers to
  framework plugins. The gtest plugin is registered at process startup
  alongside the other static plugins.
- `BuildConfiguration::test_frameworks` is a list of `TestFramework`
  entries that name a framework (e.g. `"gtest"`) and optionally pin a
  source package. When a `TestFramework` has no explicit
  `.package`, the framework's `default_package()` is consulted and
  recorded in `cppup.lock` so the next sync resolves to the same
  source.

The interface surface is roughly:

```cpp
struct TestBuildFlags {
  std::vector<std::filesystem::path> include_paths;
  std::vector<std::filesystem::path> library_paths;
  std::vector<std::string>           libraries;
  std::vector<std::string>           link_flags;
};

class TestFrameworkPlugin {
 public:
  virtual std::string_view  name() const noexcept = 0;
  virtual std::expected<TestBuildFlags, std::string>
                            build_and_get_flags(const fs::path& package_root,
                                                const fs::path& cache_dir,
                                                CommandRunner&  runner)        = 0;
  virtual std::vector<std::string> list_test_cases(const fs::path& binary,
                                                   std::string_view filter,
                                                   CommandRunner&  runner)     = 0;
  virtual int               run(const fs::path& binary, std::string_view filter,
                                CommandRunner& runner)                         = 0;
  virtual std::optional<Package> default_package() const                       = 0;
};
```

This is intentionally not part of the cross-`dlopen` C ABI yet. The
shape is likely to change once the host's package-source plugin path
matures enough to also drive test-framework binaries, at which point a
`cppup_test_system_vtable_v1` can freeze.

`cppup test [filter]` walks `config.tests`, looks each entry's
`framework` up in `TestFrameworkRegistry`, and calls
`plugin->run(binary, filter, runner)`. The plugin translates `filter`
into its native spelling (e.g. gtest's `--gtest_filter=<glob>`), so the
host stays out of framework-specific syntax. Tests with an empty
`framework` are exec'd directly; if a `filter` is supplied they are
skipped with a warning, since there is no plugin to translate it.

## 12. Lockfile integration

The plugin layer contributes one piece of state to `cppup.lock`: when a
`TestFramework` entry in `config.test_frameworks` has no explicit
`.package`, the framework plugin's `default_package()` is resolved at
`cppup lock` time and emitted as a regular `[[package]]` entry. This
keeps "tests use gtest" reproducible across machines without forcing
the user to maintain the gtest source URL by hand.

The plugin *identity* (which plugin produced the default) is not
recorded in `cppup.lock`. A rebuild after a plugin update may resolve
to a different default package. This is acknowledged drift; explicit
`.package` pinning is the escape hatch.

The `selected_registry` selection key (`cppup registry set <location>`)
likewise feeds plugin-managed package sources, but registry-aware
fetch is still on the deferred list — see
[docs/packages.md](packages.md).

## 13. Future extensions (non-binding)

- **Out-of-process plugins.** A second descriptor kind exposing
  plugins over stdin/stdout JSON-RPC. Lets untrusted plugins run in a
  child process. The in-process ABI in §3 is unaffected — both kinds
  coexist.
- **Plugin dependency resolution.** Topologically sort `installed.toml`
  by `[plugin.dependencies].plugins`.
- **Signing.** Add `signature = "ed25519:..."` to the manifest and a
  set of trusted keys under `.cppup/plugins/keys/`.
- **`cppup plugin update`.** Re-fetch from the recorded `source` and
  re-add with `--replace`.

## 14. Open issues

None blocking v1 implementation. To revisit before v1.1:

- Logger configuration: `create(const char* config_toml)` is a string
  blob. Consider a more structured way to pass logger-specific config
  from the user's `BuildConfiguration` into the plugin.
- Whether `installed.toml` should also live at a user-global path
  (`$XDG_CONFIG_HOME/cppup/plugins/`) for cross-project plugins.
