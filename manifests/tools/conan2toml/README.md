# conan2toml — conanfile.py → package.toml converter

A small Python tool that uses the **real Conan v2 Python API** to load a
`conanfile.py`, then snapshots the resulting recipe state into a cppup
`package.toml`. Designed to bootstrap the cppup registry by harvesting
Conan Center.

## Status

Design sketch — not yet implemented. Tracked in issue #47.

## Why use the real Conan API

Original sketch proposed a mock `conan` SDK that records attribute
assignments. Discarded in favor of just installing Conan and importing
its public Python entry points:

- **Correctness for free.** Conan already knows how to resolve
  `conandata.yml`, evaluate `requirements()`, expand `Version`, walk
  `cpp_info` components, apply `default_options`. Reimplementing any of
  that as a mock is a tax we keep paying every time CCI's recipe style
  shifts.
- **Recipes get more sophisticated, not less.** Conan v2 recipes call
  `tools.cmake.CMakeToolchain`, `tools.scm.Git`, `tools.files.get` —
  the API surface a mock would have to cover keeps growing. Mocking
  this is a maintenance pit.
- **Conan is fine as a build-time dep.** `conan2toml` is a migration /
  bootstrap tool, not part of cppup's runtime. The cppup C++ client
  still has zero Python deps.

The trade-off: the converter needs a working Conan installation in
whatever environment it runs in.

## Devcontainer setup

The dev environment is the Fedora-based `.devcontainer/Dockerfile`.
Additions needed:

```dockerfile
# in .devcontainer/Dockerfile after the existing dnf install block:
RUN dnf install -y python3 python3-pip && dnf clean all
RUN pip install --no-cache-dir uv

# Conan itself is installed per-project via uv in a venv that
# manifests/tools/conan2toml/ owns — not globally. Keeps the
# devcontainer agnostic to conan version pinning.
```

`conan2toml` pins its own conan version in `pyproject.toml`:

```toml
[project]
name = "conan2toml"
requires-python = ">=3.11"
dependencies = [
  "conan>=2.5,<3",
  "tomli_w",
  "pyyaml",
]
```

## Approach

1. Use `conan.api.conan_api.ConanAPI` (or the lower-level
   `conans.client.loader.ConanFileLoader`) to load the recipe in the
   same way `conan install` would.
2. Inspect the loaded `ConanFile` instance: class attributes for
   metadata, the resolved `requires` / `build_requires` / `test_requires`
   lists, the merged `options` / `default_options`, the evaluated
   `cpp_info` (when available).
3. Read the sibling `conandata.yml` directly — Conan already does this,
   but we want the per-version `sources` block (URLs, sha256s, patches)
   in a structured form that maps 1:1 to our `[source]` table.
4. Run mapper → cppup schema dict → deterministic TOML emit.

Class-level attributes (`name`, `version`, `license`, `url`, `homepage`,
`description`, `topics`, `settings`, `options`, `default_options`,
`generators`, `exports_sources`) come straight off the class.

## What translates cleanly

| Conan source                              | cppup field                            |
|-------------------------------------------|----------------------------------------|
| `name`, `version`                         | `[package].name`, `version`            |
| `license`                                 | `[package].license`                    |
| `homepage`                                | `[package].homepage`                   |
| `url`                                     | `[package].repository`                 |
| `description`                             | `[package].description`                |
| `requires(...)` (after resolution)        | `[dependencies]`                       |
| `build_requires(...)` / `tool_requires(...)` | `[build-dependencies]`              |
| `test_requires(...)`                      | `[test-dependencies]`                  |
| `options` / `default_options`             | `[features]`                           |
| `settings = "os", "arch"`                 | `[platforms]`                          |
| `conandata.yml.sources[<v>].url` + `sha256` | `[source]` (tar/zip/http + checksum) |
| `conandata.yml.patches[<v>]`              | `[source.patches]` (URL references — see below) |

## Patches: serve from Conan's repo, don't vendor

Original plan was a sibling `patches/` directory next to every
`package.toml`. Better idea: **patches live in the
`conan-center-index` git repo and we reference them by URL**. The
converter emits:

```toml
[[source.patches]]
url         = "https://raw.githubusercontent.com/conan-io/conan-center-index/<commit>/recipes/fmt/all/patches/10.2.1-fix-something.patch"
sha256      = "abc123..."
description = "Backport CMake 4 compat from upstream"
strip       = 1
```

Wins:
- No file copies; `recipes/` stays text-only, every `package.toml` is
  self-contained.
- The URL pins to a *commit* (not a branch), so the patch is
  reproducible even if Conan Center rewrites or removes the file.
- The sha256 makes integrity verifiable without trusting GitHub.
- Eliminates a whole category of converter bugs ("we forgot to copy
  patch X").

The cppup client gains a small responsibility: fetch + verify + apply
patches during the source step. That's a few hundred lines, not a
schema redesign.

## What still does NOT translate

Even with the real Conan API, some recipe behavior is irreducibly
imperative and lives in method bodies. The converter surfaces these as
a TODO block at the top of the emitted file:

```toml
# TODO(conan2toml): manual review needed for:
#   - build():    custom cmake.configure() args derived from settings.compiler
#   - package():  copy() patterns deviate from standard CMake install
#   - package_info(): cpp_info.components defined imperatively
```

Tactic: render whatever Conan's own `cpp_info` evaluation gives us
(`libs`, `system_libs`, `frameworks`, `includedirs`, `libdirs`) into
`[[exports]]` entries. That covers most "normal" packages. The TODO
fires when the recipe does something we can't snapshot — e.g. setting
`cpp_info.libs` conditionally on `self.settings.os`.

## CLI

```
$ conan2toml path/to/conanfile.py -o path/to/package.toml
$ conan2toml --batch conan-center-index/recipes/ -o manifests/recipes/ --emit-index
```

Batch mode walks Conan Center's `recipes/<name>/<version>/` layout,
writes a parallel tree under `manifests/recipes/`, and patches
`manifests/index.yaml` with each new version entry. Resume-on-failure
keeps a `.conan2toml-progress` file so re-running picks up where it
stopped.

## Project layout (when implemented)

```
tools/conan2toml/
├── pyproject.toml          # uv-managed, conan + tomli_w + pyyaml
├── src/conan2toml/
│   ├── __init__.py
│   ├── __main__.py         # CLI entry
│   ├── loader.py           # wraps conan.api.conan_api.ConanAPI
│   ├── conandata.py        # parse conandata.yml
│   ├── mapper.py           # ConanFile snapshot → cppup schema dict
│   ├── emitter.py          # deterministic TOML emit (stable key order)
│   ├── index.py            # patch manifests/index.yaml
│   └── batch.py            # walk CCI, resume, report
└── tests/
    ├── fixtures/           # small real conanfiles + expected output
    └── test_roundtrip.py   # regenerate fixture, assert byte-identical
```

Built with `uv` per project Python policy; ruff-clean; gitleaks-clean;
tests cover at least: zlib, fmt, spdlog, openssl, boost (components).
