# Dependency module (design preview)

> **Status:** This module is a design-stage component. The SQLite
> database, semver resolver, and `PackageManager` types defined here
> exist as compilable units but are **not yet wired into any CLI
> command**. The actual user-facing dependency model — manifest in
> `build.cpp`, lockfile in `cppup.lock`, on-disk cache in
> `.cppup/packages/` — is implemented in
> [src/core/cli/commands/lockfile.cpp](../cli/commands/lockfile.cpp) and
> [src/core/package/](../package/) and documented in
> [docs/packages.md](../../../docs/packages.md).
>
> The pieces here will become load-bearing once the package-registry
> path lights up (currently stubbed; see
> [docs/packages.md](../../../docs/packages.md) "Out of scope").

The design goal is to give registry-backed resolution a persistent
local index (so successive `cppup` invocations don't re-walk the same
graph) and to provide semver constraint evaluation that the manifest /
lockfile machinery can call into.

## Components

| File | Purpose |
|---|---|
| [`database.hpp`](database.hpp), [`database.cpp`](database.cpp) | SQLite schema and CRUD for installed packages, declared dependencies, and registry cache. |
| [`resolver.hpp`](resolver.hpp) | `VersionConstraint`, `DependencyRequirement`, and the resolver entry point. |
| [`package_manager.hpp`](package_manager.hpp), [`package_manager.cpp`](package_manager.cpp) | High-level install / remove / list operations layered over the database and resolver. |

## Version constraints

Constraint syntax is shared with the manifest format
([../../../manifests/README.md](../../../manifests/README.md)):

| Syntax | Meaning |
|---|---|
| `1.2.3` | exact |
| `^1.2.3` | caret — `>=1.2.3 <2.0.0` |
| `~1.2.3` | tilde — `>=1.2.3 <1.3.0` |
| `>=1.2.3`, `<=1.2.3`, `>1.2.3`, `<1.2.3` | bounds |
| `>=1.2.3 <2.0.0` | range |

## Database schema (`packages.db`)

The schema is kept TOML-able and forward-compatible. It is read /
written through `DependencyDatabase`:

```sql
CREATE TABLE packages (
    name TEXT NOT NULL,
    version TEXT NOT NULL,
    description TEXT,
    homepage TEXT,
    repository_url TEXT,
    license TEXT,
    install_path TEXT,
    checksum TEXT,
    install_time INTEGER,
    is_dev_dependency BOOLEAN DEFAULT 0,
    PRIMARY KEY (name, version)
);

CREATE TABLE dependencies (
    package_name TEXT NOT NULL,
    package_version TEXT NOT NULL,
    dependency_name TEXT NOT NULL,
    version_constraint TEXT,
    dependency_type TEXT DEFAULT 'runtime',
    PRIMARY KEY (package_name, package_version, dependency_name, dependency_type),
    FOREIGN KEY (package_name, package_version) REFERENCES packages(name, version) ON DELETE CASCADE
);

CREATE TABLE registry (
    name TEXT PRIMARY KEY,
    latest_version TEXT,
    description TEXT,
    repository_url TEXT,
    available_versions TEXT,
    last_updated TEXT
);
```

## Why it isn't wired up yet

The current dependency model deliberately keeps the source of truth in
two files committed to the repo:

- `build.cpp` (`config.packages` + `PackageInfo::dependencies`) is the
  manifest.
- `cppup.lock` is the resolved closure.

Resolving registry queries against a per-machine SQLite database makes
sense once there *is* a registry; until then, walking the dependency
graph at `cppup lock` time directly off `build.cpp` is simpler and
keeps state in the repo where reviewers can see it.

When the registry path lights up, this module will sit between the
registry client and the lockfile writer, caching version listings and
resolving constraints. The API shape is therefore tested in isolation
even though no CLI calls it today.
