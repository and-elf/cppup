"""Patch `manifests/index.yaml` to register a new package version.

The index file is the registry's table of contents (`manifests/INDEX.md`
describes the shape). After the emitter writes a fresh
`recipes/<name>/<ver>/package.toml`, the converter calls
`upsert_version` to register / replace that version's entry and pick the
new `latest`.
"""

from __future__ import annotations

from datetime import UTC, datetime
from pathlib import Path
from typing import Any


def upsert_version(
    index_path: Path,
    *,
    name: str,
    version: str,
    manifest_relpath: str,
    manifest_sha256: str,
    source_url: str,
    source_ref: str,
    kind: str,
    description: str | None = None,
    license_name: str | None = None,
    homepage: str | None = None,
    repository: str | None = None,
) -> None:
    import yaml

    data: dict[str, Any] = {}
    if index_path.exists():
        loaded = yaml.safe_load(index_path.read_text())
        if isinstance(loaded, dict):
            data = loaded
    data.setdefault("schema", 1)
    registry = data.setdefault("registry", {})
    registry["generated_at"] = datetime.now(tz=UTC).isoformat(timespec="seconds")
    packages = data.setdefault("packages", {})
    pkg = packages.setdefault(name, {})
    pkg.setdefault("kind", kind)
    if description and not pkg.get("description"):
        pkg["description"] = description
    if license_name and not pkg.get("license"):
        pkg["license"] = license_name
    if homepage and not pkg.get("homepage"):
        pkg["homepage"] = homepage
    if repository and not pkg.get("repository"):
        pkg["repository"] = repository
    versions = pkg.setdefault("versions", {})
    versions[version] = {
        "manifest": manifest_relpath,
        "manifest_sha256": manifest_sha256,
        "source_url": source_url,
        "source_ref": source_ref,
        "yanked": False,
        "deprecated": None,
    }
    pkg["latest"] = _pick_latest(versions.keys(), current=pkg.get("latest"), added=version)
    index_path.write_text(yaml.safe_dump(data, sort_keys=True, default_flow_style=False))


def _pick_latest(versions, *, current: str | None, added: str) -> str:
    candidates = list(versions)
    candidates.sort(key=_version_key, reverse=True)
    best = candidates[0]
    if current is None:
        return best
    if _version_key(added) > _version_key(current):
        return added
    return current if current in versions else best


def _version_key(v: str) -> tuple:
    parts: list[Any] = []
    for chunk in v.split("."):
        digits = "".join(c for c in chunk if c.isdigit())
        suffix = "".join(c for c in chunk if not c.isdigit())
        parts.append((int(digits) if digits else 0, suffix))
    return tuple(parts)
