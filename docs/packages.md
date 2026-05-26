# The cppup.lock file

`cppup.lock` is the single project-root file that pins everything `cppup
build` needs to reproduce on another machine. It contains two
independent kinds of state:

1. **Package resolution** — one `[[package]]` entry per reachable
   dependency, derived from `build.cpp`'s `config.packages`.
2. **Selection** — the active toolchain and profile, as top-level
   `selected_toolchain` / `selected_profile` keys, set by `cppup
   toolchain select` / `cppup profile select`.

The two halves are independent: writing a selection never disturbs the
package list, and `cppup package lock` regenerating the package list
preserves the selection.

# Packages, lockfile, and local cache

cppup separates package management into three explicit layers. Each
answers one question and is owned by one source of truth.

## The three layers

| Layer | File / directory | Source of truth | Committed? |
|------:|------------------|-----------------|-----------:|
| **Manifest** | `build.cpp` (`config.packages`) | Hand-authored intent | Yes |
| **Lockfile** | `cppup.lock` at project root | Resolved state derived from `build.cpp` | **Yes** |
| **Local cache** | `.cppup/packages/` and `.cppup/packages/registry.txt` | Materialized package contents on this machine | No |

- **Manifest** declares *what* the project depends on, with as much or
  little version pinning as the author wants.
- **Lockfile** captures *which exact resolution* of those declarations
  should be installed — including resolved versions, source URLs, and
  (eventually) commit hashes and checksums. It is the contract between
  declared dependencies and what gets installed.
- **Local cache** is the on-disk materialization. It is rebuilt by
  `cppup package sync` from the lockfile and is never the source of
  truth for what packages a project depends on.

## Recommended team workflow

```
git clone <project>
cd <project>
cppup sync     # materializes .cppup/packages from cppup.lock
cppup build    # auto-runs sync first if cppup.lock is present
```

That is the entire onboarding for a fresh checkout - mirrors
`cargo build` / `uv sync` / `npm install`. The first `cppup sync` is
optional; `cppup build` will sync on its own when `cppup.lock` exists.

Day-to-day:

1. **Edit `build.cpp`** to add or remove a package.
2. **Run `cppup lock`** to regenerate `cppup.lock`.
3. **Commit both** `build.cpp` and `cppup.lock`.
4. **CI** runs `cppup build` against the committed `cppup.lock` - the
   auto-sync proves the lockfile reproduces.

`cppup package add` stays as the convenience path for ad-hoc local
installs; it does not currently mutate `build.cpp` or `cppup.lock`. The
`cppup package lock` / `cppup package sync` subcommands also still
exist for scripts that already invoke them.

### Install scope: project vs user

Both `cppup package add` and `cppup toolchain add` accept `-u` /
`--user` to install into a per-user data directory shared across
projects, instead of the project-local `.cppup/`:

```bash
# Project-local (default) — lands in <repo>/.cppup/{packages,toolchains}/
cppup package add --name fmt --git https://github.com/fmtlib/fmt.git
cppup toolchain add --name clang-19

# User-wide — lands in $XDG_DATA_HOME/cppup/ (if set), else $HOME/.cppup/
cppup package add --name fmt --git https://github.com/fmtlib/fmt.git --user
cppup toolchain add --name clang-19 -u
```

`cppup package list` and `cppup toolchain list` enumerate both scopes
and tag each entry with `(project)` or `(user)`. `cppup package remove`
and `cppup toolchain remove` search both scopes; if the same name
exists in both, the project copy is removed first.

User scope follows the XDG Base Directory Specification: it uses
`$XDG_DATA_HOME/cppup/` when `XDG_DATA_HOME` is set and non-empty, and
falls back to `$HOME/.cppup/` otherwise. `--user` errors out when
neither environment variable is available.

The auto-sync from `cppup.lock` is project-scoped — packages declared
in `build.cpp` always materialize into the project's `.cppup/`. User
scope is for ad-hoc, cross-project installs (e.g., a custom toolchain
you reuse from many repos) and is independent of the lockfile.

## Declaring transitive dependencies

Every `from_*` helper takes a final `dependencies` parameter listing
direct children. The manifest *is* the graph - there is no central
registry that knows what fmt depends on.

```cpp
#include <cppup/configuration.hpp>
using namespace cppup::configuration;
using namespace cppup::configuration::package_helpers;

extern "C" BuildConfiguration configure()
{
  BuildConfiguration config;
  config.packages.push_back(from_git("fmt", "https://github.com/fmtlib/fmt.git", "10.2.1",
                                     {from_git("zlib", "https://github.com/madler/zlib.git")}));
  return config;
}
```

`cppup lock` walks `config.packages` plus each package's
`PackageInfo::dependencies` recursively. The walker dedupes by name
(first occurrence wins), detects cycles, and writes one `[[package]]`
entry per reachable node with the direct child names recorded in
`dependencies = [...]`.

## `cppup lock` (also `cppup package lock`)

- Loads `build.cpp` and walks the full dependency graph.
- Writes `cppup.lock` at the project root.
- Deterministic: packages emit in lexicographic order with a fixed key
  order, so unchanged inputs yield byte-identical output.
- Project-scoped: packages in `.cppup/packages/registry.txt` that aren't
  reachable from `config.packages` are not written.

## `cppup sync` (also `cppup package sync`)

- Reads `cppup.lock`.
- For each entry: fetches the package into `.cppup/packages/<name>/`
  if the directory is missing or empty.
- Reconciles `.cppup/packages/registry.txt` with the lockfile so the
  metadata matches what's on disk.
- **Idempotent**: a second run is a no-op when state already matches.
- Repairs partial state: deleting `.cppup/packages/<name>/` and
  re-running `sync` re-fetches it; deleting the metadata while keeping
  the directory restores the registry record from the lockfile.

## `cppup build` auto-sync

When `cppup.lock` is present at the project root, `cppup build` runs the
equivalent of `cppup sync` before configuring the build. This is the
mechanism that makes `git clone && cppup build` work for a fresh
checkout without an explicit sync step. Sync is idempotent, so the
overhead on an already-materialized project is reading and parsing the
lockfile and one stat per package.

Projects without a `cppup.lock` (e.g. the cppup tree itself, or a
project that hasn't opted in yet) see no behaviour change.

## Selection: toolchain and profile

The lockfile also persists the **active selection** — which toolchain
and which profile a fresh `cppup build` in this directory should use.
This lives alongside the package list as top-level keys; the two halves
are read and written independently.

```text
# cppup.lock - DO NOT EDIT
version = 1
selected_toolchain = "clang++"
selected_profile   = "debug"

[[package]]
...
```

Empty selections are omitted from the file. A lockfile with no selection
keys is byte-identical to a `cppup package lock` output that had no
selection in the first place.

### Setting the selection

```
cppup toolchain select clang++
cppup profile select debug
```

Both commands round-trip through the lockfile: they read the file (if
present), update one key, and rewrite it. The package list is preserved.
Either command creates `cppup.lock` if it doesn't already exist.

### Precedence

Two resolutions happen on every `cppup build`. The **early** resolution
runs before `build.cpp` is compiled, so `when_toolchain` /
`when_profile` blocks inside `configure()` see the correct value. The
**final** resolution runs after `configure()` returns and may use the
configuration's own default.

Early resolution (toolchain):

1. `--toolchain` CLI flag
2. `selected_toolchain` in `cppup.lock`
3. `$CXX`, then `$CC`
4. Hardcoded `"g++"`

Final resolution (toolchain) inserts the project default between (2)
and (3):

1. `--toolchain` CLI flag
2. `selected_toolchain` in `cppup.lock`
3. `config.toolchain->name` set by `build.cpp`
4. `$CXX`, then `$CC`
5. Hardcoded `"g++"`

Profile resolution is simpler — `--profile` > `selected_profile` >
whatever the configuration's `ProfileProcessor` picks. An unknown
profile name is a hard error when the configuration declares profiles
at all.

### Exported into the build environment

Before compiling `build.cpp`, cppup exports the early selection as
process-environment variables that the build.cpp DSO inherits via
`dlopen`:

- `CPPUP_ACTIVE_TOOLCHAIN` — always set.
- `CPPUP_ACTIVE_PROFILE` — set only when a profile is active; otherwise
  explicitly unset so `when_profile()` correctly doesn't fire on absence.

`active_toolchain()` / `active_profile()` in `<cppup/runtime.hpp>` read
these. `when_toolchain` / `when_profile` are the typical entry points.

### Legacy `.cppup/toolchain.txt`

Before the lockfile existed, a stray `.cppup/toolchain.txt` file held
the selected toolchain. `cppup build` and `cppup toolchain select` both
migrate this file into the lockfile on first run and remove it. No user
action is required; the migration is idempotent.

### What is *not* persisted

- **Target architecture** is not a separate selection: the toolchain
  name determines it (e.g. `aarch64-linux-gnu-g++` ⇒ arm64). Use
  `when_toolchain` for target-arch-specific config.
- **Host architecture** is detected at configure-compile time and
  exposed via `when_x86_64` / `when_arm64` in `platform.hpp`.
- **`--verbose`, build flags, output directories** — these are
  per-invocation, not selection.

## Lockfile format

`cppup.lock` is a small line-based format with a version header,
optional top-level selection keys, and one `[[package]]` section per
entry:

```text
# cppup.lock - DO NOT EDIT
# Generated by `cppup package lock`. Run `cppup package sync` to reproduce.
version = 1
selected_toolchain = "clang++"
selected_profile = "debug"

[[package]]
name = "fmt"
version = "10.2.1"
source = "git"
url = "https://github.com/fmtlib/fmt.git"
git_branch = "10.2.1"
git_commit = ""
subdirectory = ""
build_system = ""
checksum = ""
dependencies = []
```

- `version` must equal the format version the binary understands (`1`).
  Unknown versions are rejected; run `cppup package lock` to regenerate.
- `selected_toolchain` / `selected_profile` are optional. Either can
  appear without the other. Malformed selection lines are silently
  treated as absent so a corrupt selection cannot block a build.
- `source` is one of `registry`, `git`, `directory`, `url`, `tar`, `zip`.
- `dependencies` lists the **direct child** names declared for this
  package in the manifest. Transitive grandchildren get their own
  `[[package]]` entries; reconstructing the full graph means following
  `dependencies` from each entry.
- Unknown per-package keys and unknown top-level keys are ignored on
  parse, so older readers survive forward-compatible additions.
- Output is deterministic: packages sort lexicographically by name, key
  order within an entry is fixed, and strings are always quoted. Two
  runs on the same input produce byte-identical output.

## How `cppup build` uses the lockfile

A single `cppup build` invocation touches the lockfile in three places:

1. **Auto-sync.** If `cppup.lock` exists, run the equivalent of
   `cppup package sync` to materialize `.cppup/packages/` from the
   `[[package]]` entries.
2. **Legacy migration.** Fold any `.cppup/toolchain.txt` into the
   lockfile's `selected_toolchain` and delete the legacy file.
3. **Selection.** Read `selected_toolchain` / `selected_profile`,
   combine with CLI flags and env fallbacks (see precedence above),
   export `CPPUP_ACTIVE_*` so `build.cpp`'s `when_*` blocks see the
   right values, and finally apply the resolved toolchain + profile to
   the loaded `BuildConfiguration`.

`compile_commands` follows the same selection path (minus the auto-sync
and legacy migration) so the generated `compile_commands.json` reflects
the active toolchain + profile.

A project with no `cppup.lock` skips all three steps — no auto-sync, no
migration, and selection falls back to `$CXX`/`$CC` and the
configuration's own defaults.

## Out of scope for the initial implementation

The following are deliberately deferred. They are open follow-ups that
build on this layer model:

- Automatic regeneration of `cppup.lock` on every `cppup build` (today
  `build` only auto-syncs; it does not re-lock).
- `cppup package add` mutating `build.cpp` or `cppup.lock`.
- Populating `git_commit` and `checksum` — these fields exist in the
  schema but are written empty until a resolution step pins them.
- Fetch support for `url`, `tar`, `zip`, and `registry` sources during
  `sync` (currently they create a placeholder directory, mirroring the
  behaviour of `package add`).
- `cppup update` install location split: system-wide `/usr/local/` by
  default, `--user` for `$XDG_DATA_HOME` plus env setup.
- Build-time resolution from the user data dir. Today the build still
  drives compilation through the project lockfile + PATH; the user
  install dir is consulted only by `package add`/`list`/`remove` and
  `toolchain add`/`list`/`remove`.
