# Add `cppup.lock` and `cppup package sync` for reproducible project package state

## Summary

`cppup package add` currently fetches/downloads packages into local cppup state and registers them in local metadata/database state, but it does not produce a committed, project-scoped record of the resolved package graph.

That leaves cppup without a lockfile equivalent to `uv.lock`, `Cargo.lock`, or `package-lock.json`.

For shared projects, this creates a gap between:

- dependency declarations in `build.cpp`
- locally installed package/database state
- actual contents of `.cppup/`

## Problem

Today, the de facto project manifest is `build.cpp` via `config.packages`, while `cppup package add` behaves more like a local install/register operation.

This means:

- there is no committed lockfile capturing exact resolved package state
- teammates cannot reliably reproduce package state from git alone
- there is no hardened contract between:
  - declared dependencies in `build.cpp`
  - what the package database says is installed
  - what actually exists in `.cppup`
- branch/tag/version-based dependencies may drift over time
- stale local cache or DB state can diverge from what the current project actually needs

## Expected behavior

cppup should support:

1. a project-root `cppup.lock` file generated from the **resolved dependency graph of the current project**
2. a `cppup package sync` command that materializes local package state from the lockfile
3. an integration-tested sync workflow that proves a fresh project clone can reproduce package state from committed files

## Scope

### Manifest
Continue treating `build.cpp` / `config.packages` as the source of project dependency intent.

### Lockfile
Add a committed `cppup.lock` that records the resolved package graph for the current project only.

The lockfile should be derived from:

- the current project manifest
- the resolver’s output for that manifest

It should **not** be generated from arbitrary local database contents unrelated to the current project.

### Sync
Add `cppup package sync` to reconcile local `.cppup` package state with `cppup.lock`.

`sync` should be idempotent and should not just be a thin loop around the current `package add` CLI behavior. It should use shared lower-level install/materialization logic appropriate for deterministic reconciliation.

## Proposed behavior

### `cppup package lock`
- resolve the project’s dependency graph from `build.cpp`
- write `cppup.lock` at project root
- include only packages required by the current project
- overwrite/update the lockfile deterministically

### `cppup package sync`
- read `cppup.lock`
- fetch/install missing packages
- register them in the local package database as needed
- verify existing local state where possible
- re-fetch or repair mismatched/missing package state
- succeed when run repeatedly without changing state

### `cppup build`
Initial implementation does not need to rewrite the lockfile automatically.

Follow-up behavior could include:
- validating that `cppup.lock` matches the current manifest
- failing with guidance to run `cppup package lock` if stale
- optional explicit lock regeneration flag later

## Lockfile contents

At minimum, `cppup.lock` should capture enough information to reproduce installs deterministically, such as:

- package name
- resolved version
- source type
- source URL / registry identity
- exact git commit for git dependencies
- selected/inferred build system if relevant to reproduction
- dependency edges / transitive graph
- checksums / integrity hashes where available

Exact format is open for design, but it should be stable and deterministic.

## Acceptance criteria

- [ ] Add a project-root `cppup.lock` format for resolved package state
- [ ] Add code to generate `cppup.lock` from the current project’s resolved dependency graph
- [ ] Ensure the lockfile is scoped to the current project only, not all packages known to the local DB
- [ ] Add `cppup package lock`
- [ ] Add `cppup package sync`
- [ ] Make `cppup package sync` idempotent
- [ ] Ensure `sync` can recreate missing local package state from `cppup.lock`
- [ ] Ensure `sync` registers/restores local package metadata consistently with installed package contents
- [ ] Detect and handle mismatches between local metadata and on-disk package state
- [ ] Document the intended roles of:
  - [ ] `build.cpp` as manifest
  - [ ] `cppup.lock` as resolved state
  - [ ] `.cppup/` as local materialized/cache state
- [ ] Document recommended team workflow for commit/clone/sync/build

## Testing requirements

This must include **integration tests**, not just unit tests.

Required coverage:

- [ ] Integration test: generate `cppup.lock` from a project with declared packages
- [ ] Integration test: fresh project state + committed `cppup.lock` + `cppup package sync` reproduces package install state
- [ ] Integration test: running `cppup package sync` twice is a no-op/idempotent
- [ ] Integration test: deleting a package directory from `.cppup` and rerunning `sync` restores it
- [ ] Integration test: local DB metadata exists but package files are missing -> `sync` repairs state
- [ ] Integration test: package files exist but metadata is missing/incomplete -> `sync` repairs state
- [ ] Integration test: stale/unrelated local package entries do not get written into `cppup.lock`
- [ ] Integration test: lockfile generation is deterministic across repeated runs for unchanged inputs

## Non-goals for initial implementation

- automatic mutation of `build.cpp`
- full manifest-editing behavior in `cppup package add`
- automatic lock regeneration on every `cppup build`
- solving every future package source type edge case up front

## Rationale

This would give cppup a clear three-layer model:

- **Manifest:** `build.cpp`
- **Lockfile:** `cppup.lock`
- **Local state/cache:** `.cppup/`

That separation should make package management reproducible, team-friendly, and less dependent on local machine history.
