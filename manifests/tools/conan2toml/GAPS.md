# conan2toml — gap analysis

What needs to exist before `conanfile.py → package.toml` can run end-to-end
on Conan Center. Grouped by where the gap lives.

## 1. Schema gaps in `package.toml`

Things Conan recipes routinely express that the current cppup schema can't
hold. Each must be either added to the schema, pushed into a build plugin,
or explicitly declared out-of-scope.

| Conan feature                          | Status      | Resolution                                  |
|----------------------------------------|-------------|---------------------------------------------|
| **Components** (one package, many libs — boost, openssl) | **missing** | Add `[[exports]]` with `provides` / `requires_internal` list. Blocker for boost/openssl/qt. |
| **`tool_requires`** vs `build_requires`| partial     | Conan v2 split; map both to `[build-dependencies]`. |
| **`settings` matrix** (compiler, build_type, libcxx) | partial | Encode as features; document mapping table. |
| **`options`** with non-bool values (`shared=True`, `fPIC=True`, `with_zlib=True`, `level=O3`) | partial | Extend `[features]` to allow `type = "enum"` + `values = [...]`. |
| **`default_options` per dependency**   | missing     | Add `[dependencies.<name>.options]`.        |
| **`generators = "CMakeDeps"`**         | n/a         | cppup picks generator from build plugin; drop. |
| **`exports_sources`** (patches in-tree)| **resolved** | Schema gets `[source.patches]` as **URL references** into the `conan-center-index` git repo (commit-pinned + sha256). No file vendoring. See `tools/conan2toml/README.md`. |
| **`build_policy`** (always/never/missing) | missing | Defer — cppup is source-build only in v1.   |
| **Custom layouts** (`cpp_info.libdirs`, multiple include roots) | partial | `[[exports]].include_paths` already plural; add `lib_paths`. |
| **`cpp_info.system_libs`** (m, pthread, dl) | missing | Add `system_libs = [...]` to `[[exports]]`. |
| **`cpp_info.frameworks`** (macOS)      | missing     | Add `frameworks = [...]` to `[[exports]]`.  |
| **`runtime` / `dll`** (Windows MSVC)   | missing     | Defer — handle when Windows is a real target. |
| **`requirements()` method** (conditional deps) | **resolved** | Real Conan API evaluates it; we snapshot the resolved list. No more mock or TODO emission. |
| **`validate()`** (refuse unsupported settings) | missing | Add `[platforms].requires` constraints; emit TODO for complex cases. |
| **`source()` body** beyond a single fetch (multi-archive, version-string substitution) | missing | Either support multi-archive `[[source]]`, or emit TODO. |
| **`conandata.yml`** (per-version URLs/patches/sha)  | partial | Real Conan API surfaces it via `self.conan_data`; mapper reads the per-version block directly. |

### Schema additions to ship before v1 of the converter

A minimum that gets us through ~70% of Conan Center without TODOs:

```toml
# components
[[exports]]
type             = "library"
name             = "boost_filesystem"
include_paths    = ["include"]
lib_paths        = ["lib"]
link_name        = "boost_filesystem"
system_libs      = ["pthread"]
frameworks       = []                # macOS
requires_internal = ["boost_system"] # depends on another export in this pkg

# enum-valued features
[features.shared]
type        = "bool"
default     = false

[features.threading]
type    = "enum"
values  = ["single", "multi"]
default = "multi"

# per-dep option pinning
[dependencies.openssl]
version = ">=3.0"
options = { shared = true, no_legacy = true }

# patches — referenced by commit-pinned URL, not vendored
[source]
type     = "tar"
url      = "..."
checksum = "sha256:..."
[[source.patches]]
url         = "https://raw.githubusercontent.com/conan-io/conan-center-index/<commit>/recipes/fmt/all/patches/10.2.1-fix.patch"
sha256      = "abc123..."
description = "Backport CMake 4 compat from upstream"
strip       = 1
```

## 2. Tooling gaps

What `conan2toml` itself needs that doesn't exist yet (slimmer now that
we use the real Conan API instead of mocking it):

- **Devcontainer additions.** `python3`, `python3-pip`, `uv` in the
  Fedora image; `conan` itself installed per-project via uv venv.
  Concretely: a 2-line `RUN dnf install ...` plus `pip install uv`.
- **Conan API wrapper.** Thin loader around
  `conan.api.conan_api.ConanAPI` (or `ConanFileLoader`) that takes a
  `conanfile.py` path and returns a loaded `ConanFile` instance with
  `conan_data` populated.
- **Mapping table.** Single Python module defining `(conan_attr) →
  (cppup_field)` — the rules from `README.md` rendered as code so they
  can be reviewed and tested.
- **Deterministic TOML emitter.** `tomli_w` doesn't pin key order; we
  need byte-stable output so regenerating the same input twice gives
  identical bytes (CI diff-guard).
- **Patch URL resolver.** Given a CCI commit + recipe path + per-version
  patch list from `conandata.yml`, build the `raw.githubusercontent.com`
  URL for each patch and fetch+sha256 it. This is small but new.
- **Index patcher.** After emitting `recipes/<name>/<ver>/package.toml`,
  compute its sha256 and upsert the version entry in `index.yaml`.
- **Batch runner.** Walk a Conan Center checkout; resume on failure;
  produce a report of which recipes converted clean / with TODOs / failed.

## 3. Cppup-side gaps

For the registry to be useful, the cppup client also needs:

- **Manifest parser** for the schema documented in `manifests/README.md`.
  Currently the existing `PackageInfo` is constructed in C++ via
  `make_package` builders — there's no TOML loader for a *registry*
  manifest yet (only the lockfile and plugin manifest parsers exist:
  [lockfile.hpp:27](src/core/cli/commands/lockfile.hpp#L27),
  [plugin/manifest.hpp:74](src/core/plugin/manifest.hpp#L74)).
- **Registry-source resolver.** `SourceType::REGISTRY` exists in the
  enum but there's no fetch path that consults `index.yaml` → downloads
  `package.toml` → verifies sha256 → hands the resulting `PackageInfo`
  to the resolver. This is the missing wiring between the registry and
  the dependency resolver.
- **Patch fetch + apply step.** New responsibility on the client now
  that patches are URL-referenced: during the source step, fetch each
  patch from its pinned URL, verify the sha256, apply with `patch -p<N>`
  (or libgit2 equivalent). A few hundred lines, no schema impact.
- **Feature/option propagation.** `DependencyRequirement` doesn't carry
  options yet ([resolver.hpp:44](src/core/dependency/resolver.hpp#L44));
  needs a `requested_features` map so `[dependencies.openssl].options`
  actually reaches the resolved openssl build.

## 4. Process gaps

- **Conan Center mirror or pinning.** The translator's output is only
  reproducible if its input is pinned. Either vendor a snapshot of
  `conan-center-index` (large but trivially deterministic) or pin to a
  specific upstream commit and re-run on schedule. Recommend: pin to a
  commit, automate the bump.
- **CI: regen + diff guard.** A job that re-runs the converter against
  the pinned CCI snapshot and fails if `manifests/recipes/` diverges
  from what's committed. Prevents drift between checked-in output and
  the converter's behavior.
- **Yanking / deprecation policy.** Index supports both fields, but
  there's no documented process for who can flip them or how an
  upstream Conan yank propagates. Out of scope to *implement* now;
  needs a one-pager before the registry takes external users.

## Critical path to a working pipeline

Minimum sequence to get *one* converted recipe (e.g. fmt) round-tripping
through the cppup client:

1. **Devcontainer**: add python3 + uv to the Fedora image. Trivial.
2. **Schema add**: `[source.patches]` (URL refs), `system_libs`,
   `frameworks`, `[features.<name>.type = "enum"]`,
   `[dependencies.<name>.options]`, `requires_internal` on `[[exports]]`.
3. **Converter v0**: Conan API loader + `conandata.yml` reader + mapper
   + deterministic emitter. Drops the previously-blocking "mock SDK"
   work item entirely — fmt + zlib + spdlog should all work first try.
4. **Index emitter**: convert → write `package.toml` → patch `index.yaml`.
5. **Cppup client**: TOML manifest parser + registry resolver path +
   patch fetch/verify/apply.
6. **Then**: components, enum options — the long tail.

Items 1-5 are the blockers; item 6 is the scope expansion that brings
Conan Center coverage from "demo" to "useful". Note: switching to the
real Conan API turned three previously-blocking items (`requirements()`
evaluation, `conandata.yml` ingestion, mock SDK) into a single small
loader.
