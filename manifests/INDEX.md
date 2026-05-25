# Registry index

The index is the registry's table of contents: one file that lists every
package, every version, and where to fetch the corresponding
`package.toml`. Clients (cppup itself, web UIs, mirror tools) hit the
index first; the manifest is fetched on demand.

## Format choice: YAML

YAML, not JSON, for the index — for three reasons:

1. **Diffable**: the index lives in git. Adding a version is a one-line
   YAML diff; in JSON it's a one-line diff plus comma churn.
2. **Human-scannable**: maintainers will read the index when triaging
   "is `boost 1.85.0` in the registry yet?".
3. **Anchors / aliases**: lets us share long URL prefixes across
   versions without templating.

A JSON view (`index.json`) is rendered alongside it for clients that
prefer a single-shot parse. Both are generated; YAML is the source of
truth.

## Top-level shape

```yaml
schema: 1
registry:
  name: cppup-main
  url:  https://registry.cppup.dev
  generated_at: 2026-05-25T00:00:00Z
  generator: conan2toml 0.1.0

packages:
  fmt:
    kind: library
    description: A modern formatting library
    license: MIT
    homepage: https://fmt.dev
    repository: https://github.com/fmtlib/fmt
    latest: 10.2.1
    versions:
      "10.2.1":
        manifest: recipes/fmt/10.2.1/package.toml
        manifest_sha256: 7c2e3a...        # integrity of the .toml itself
        source_url: https://github.com/fmtlib/fmt.git
        source_ref: "10.2.1"
        source_checksum: null              # git: pinned by commit in manifest
        yanked: false                       # removed without breaking refs
        deprecated: null                    # or "replaced by X"
      "10.1.1":
        manifest: recipes/fmt/10.1.1/package.toml
        manifest_sha256: 1a8b09...
        yanked: false

  zlib:
    kind: library
    latest: "1.3.1"
    versions:
      "1.3.1":
        manifest: recipes/zlib/1.3.1/package.toml
        manifest_sha256: c41d77...
        source_url: https://zlib.net/zlib-1.3.1.tar.gz
        source_checksum: sha256:9a93b2b7...
```

### Field reference

| Field                  | Required | Meaning                                                      |
|------------------------|:--------:|--------------------------------------------------------------|
| `schema`               | yes      | Index schema version. Bumped on breaking change.            |
| `registry.name`        | yes      | Human label, e.g. `cppup-main`.                              |
| `registry.url`         | yes      | Base URL clients prepend to `manifest:` paths.               |
| `registry.generated_at`| yes      | ISO-8601 UTC; clients can show staleness.                    |
| `registry.generator`   | yes      | Tool + version that produced the index.                      |
| `packages.<name>.kind` | yes      | `library` \| `tool` \| `toolchain` \| `plugin`. Mirrors manifest. |
| `packages.<name>.latest` | yes    | Convenience pointer to the newest non-yanked version.        |
| `versions.<v>.manifest`| yes      | Path under `registry.url` to fetch the `package.toml`.       |
| `versions.<v>.manifest_sha256` | yes | Integrity of the `.toml` bytes — must match before parsing. |
| `versions.<v>.source_url` | yes   | Where the upstream sources come from. Lets the index serve simple search/audit without fetching manifests. |
| `versions.<v>.source_ref` | git only | Tag or branch.                                            |
| `versions.<v>.source_checksum` | tar/zip/http only | `sha256:...` of the archive.                  |
| `versions.<v>.yanked`  | yes      | `true` = clients must refuse a fresh install but may use a previously-locked entry. |
| `versions.<v>.deprecated` | optional | String message or `null`. Surfaced as a warning by `cppup install`. |

## Sharding

A flat `index.yaml` is fine up to ~1k packages. Past that, shard by
first letter or by SHA prefix and replace the top-level `packages:`
with:

```yaml
shards:
  a: shards/a.yaml
  b: shards/b.yaml
  ...
```

Decision: ship flat for v1, revisit when the registry crosses 500
entries.

## Integrity model

Three checksums, three jobs:

1. `versions.<v>.manifest_sha256` — pins the manifest TOML. Verified
   *before* parsing the manifest, so a tampered TOML can't redirect
   sources.
2. `package.toml` `[source].checksum` — pins the archive (tar/zip/http
   only; git is pinned by commit).
3. `cppup.lock` `checksum` — pins the *resolved* artifact for the
   consumer. Written by the client after a successful resolve.

The index is signed (detached signature, `index.yaml.sig`) once the
registry has a real key story; out of scope for v1.

## Update workflow

Editing the index by hand is allowed only for: yanking, deprecating,
and updating `latest`. Adding a version is always done by running the
converter on a new upstream `conanfile.py` and letting it patch the
index:

```
conan2toml --batch conan-center-index/recipes/fmt --emit-index
  ↳ writes manifests/recipes/fmt/10.2.1/package.toml
  ↳ patches manifests/index.yaml with the new versions.<v> entry
```

CI re-renders `index.json` from `index.yaml` on every push.
