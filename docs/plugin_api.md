# cppup Plugin API — Specification v1

Status: **Draft** — tracks issue #26.
Target: cppup 0.2.0.

This document is the single source of truth for the plugin system. Tests
and implementation derive from this spec; if the spec is wrong, fix the
spec first.

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
  this (see §11) but v1 loads plugins in-process under full trust.
- Cross-version binary compatibility across major cppup releases. A
  plugin stamped for `0.2.x` is not expected to load under `0.3.0`.
- A package format / signing infrastructure. The manifest carries hashes
  for tamper detection; trust establishment is the user's problem.
- Automatic resolution of plugin dependencies on other cppup plugins.
  The manifest's `[plugin.dependencies].plugins` is informational in v1.

## 2. Plugin kinds

Three kinds. Each maps to an existing cppup concept (the plugin layer
does not invent new abstractions; it lets external code satisfy
existing ones via a stable C ABI).

| Kind enum                    | Existing concept                                 | Lives in                                                       |
| ---                          | ---                                              | ---                                                            |
| `CPPUP_KIND_BUILD_SYSTEM`    | `cppup::buildsystems::*Package`                  | [src/core/buildsystems/](../src/core/buildsystems/)            |
| `CPPUP_KIND_PACKAGE_SOURCE`  | `cppup::package::PackageType`                    | [src/core/package/](../src/core/package/)                      |
| `CPPUP_KIND_LOGGER`          | `cppup::logger::LoggerType`                      | [src/core/logger/](../src/core/logger/)                        |

A "config-hook" kind is **not** part of the plugin ABI. A plugin that
needs to augment a user's `BuildConfiguration` ships a header (e.g.
`include/foo_plugin.hpp`) with regular C++ functions; the user
`#include`s it and calls it explicitly from `cppup.cpp`. Plugins never
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

### 3.4 Vtables (initial sketch)

Status types and shared structs:

```c
typedef enum {
  CPPUP_OK = 0,
  CPPUP_ERR_GENERIC = 1,
  CPPUP_ERR_NOT_SUPPORTED = 2,
  CPPUP_ERR_IO = 3,
  // ... grow as needed; never renumber
} cppup_status;

// Error detail accessor: when a vtable function returns non-zero,
// the host calls this to retrieve the last error message for the
// instance. The returned pointer is owned by the plugin and must
// remain valid until the next call on the same instance.
typedef const char* (*cppup_last_error_fn)(void* instance);
```

Build system:

```c
typedef struct {
  const char*          name;           // "ninja", "meson", ...
  cppup_last_error_fn  last_error;

  bool (*detects)(const char* source_path);
  void* (*create)(const cppup_package_info* info);
  void  (*destroy)(void* instance);
  cppup_status (*build)(void* instance, const char* source_path);

  // Flag accessors: caller-allocated buffer; if too small, function
  // returns the required size and writes nothing.
  size_t (*get_compile_flags)(void* instance, char* out, size_t cap);
  size_t (*get_link_flags)(void* instance, char* out, size_t cap);
  size_t (*get_include_paths)(void* instance, char* out, size_t cap);
  size_t (*get_library_paths)(void* instance, char* out, size_t cap);
} cppup_build_system_vtable_v1;
```

Package source:

```c
typedef struct {
  cppup_source_type    accepted_type;   // matches cppup::configuration::SourceType
  cppup_last_error_fn  last_error;

  void* (*create)(const cppup_package_info* info);
  void  (*destroy)(void* instance);
  cppup_status (*resolve_source)(void* instance, char* out_path, size_t cap);
  void  (*set_command_executor)(void* instance, cppup_cmd_exec_v1* exec);
  void  (*set_cache)(void* instance, cppup_cache_v1* cache);
} cppup_package_source_vtable_v1;
```

Logger:

```c
typedef struct {
  const char*          name;            // "json", "syslog", ...
  cppup_last_error_fn  last_error;

  void* (*create)(const char* config_toml);
  void  (*destroy)(void* instance);
  void  (*log)(void* instance, uint8_t level, const char* message, size_t len);
} cppup_logger_vtable_v1;
```

The full ABI header is exhaustive in [include/cppup/plugin/abi.h](#)
(to be created by the implementation step). The sketches above are
normative for the structure but not for every field — `cppup_cmd_exec_v1`,
`cppup_cache_v1`, and `cppup_package_info` follow the same pattern.

### 3.5 String and buffer rules

- All strings crossing the ABI are NUL-terminated UTF-8, except where
  explicitly paired with a length (`log`).
- Out-buffers follow the **two-call pattern**: pass `out=NULL, cap=0`
  to query required size; pass a sufficient buffer to receive data.
  If `cap` is non-zero but too small, the function writes nothing and
  returns the required size.
- Pointers passed by the host into the plugin are valid only for the
  duration of the call.
- Pointers returned by the plugin must remain valid at least until
  the next call into the same instance or the plugin SO, whichever
  comes first (i.e. plugins own their return buffers).

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

This replaces the stub in [src/core/cli/commands/plugin.cpp](../src/core/cli/commands/plugin.cpp).

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

Reads `installed.toml` only. Output for each installed plugin: name,
version, source it was added from, install date, and the list of
entries (id + kind) from the on-disk sidecar manifest. Does not
`dlopen` anything.

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

Per the [assertion refactor](../memory/) policy:

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
out-of-process model (§11) addresses this later.

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

## 11. Future extensions (non-binding)

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

## 12. Open issues

None blocking v1 implementation. To revisit before v1.1:

- Logger configuration: `create(const char* config_toml)` is a string
  blob. Consider a more structured way to pass logger-specific config
  from the user's `BuildConfiguration` into the plugin.
- Whether `installed.toml` should also live at a user-global path
  (`$XDG_CONFIG_HOME/cppup/plugins/`) for cross-project plugins.
