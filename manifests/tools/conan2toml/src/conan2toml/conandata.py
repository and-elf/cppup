"""Parse a Conan recipe's `conandata.yml`.

Conan stores per-version source URLs, checksums, and patch lists in a
sibling `conandata.yml` next to the `conanfile.py`. The shape is loose
(`url:` may be a string or a list of mirrors; `patches:` is optional),
so the mapper consumes the structured pair below instead of raw dicts.
"""

from __future__ import annotations

from dataclasses import dataclass, field
from pathlib import Path
from typing import Any


class ConandataError(RuntimeError):
    pass


@dataclass(frozen=True)
class SourceEntry:
    url: str
    sha256: str


@dataclass(frozen=True)
class PatchEntry:
    file: str
    description: str = ""
    type: str = ""
    strip: int = 1
    base_path: str = field(default="")


def load_conandata(path: Path) -> dict[str, Any]:
    if not path.exists():
        raise ConandataError(f"conandata.yml not found: {path}")
    import yaml

    try:
        data = yaml.safe_load(path.read_text()) or {}
    except yaml.YAMLError as exc:
        raise ConandataError(f"failed to parse {path}: {exc}") from exc
    if not isinstance(data, dict):
        raise ConandataError(f"conandata.yml root must be a mapping: {path}")
    return data


def source_for_version(data: dict[str, Any], version: str) -> SourceEntry:
    sources = data.get("sources") or {}
    entry = sources.get(version)
    if entry is None:
        raise ConandataError(f"no source entry for version {version!r}")
    url = entry.get("url")
    if isinstance(url, list):
        if not url:
            raise ConandataError(f"empty url list for version {version!r}")
        url = url[0]
    if not isinstance(url, str):
        raise ConandataError(f"missing or invalid url for version {version!r}")
    sha = entry.get("sha256")
    if not isinstance(sha, str) or not sha:
        raise ConandataError(f"missing sha256 for version {version!r}")
    return SourceEntry(url=url, sha256=sha)


def patches_for_version(data: dict[str, Any], version: str) -> list[PatchEntry]:
    patches = (data.get("patches") or {}).get(version) or []
    out: list[PatchEntry] = []
    for raw in patches:
        if not isinstance(raw, dict):
            continue
        out.append(
            PatchEntry(
                file=str(raw.get("patch_file", "")),
                description=str(raw.get("patch_description", "")),
                type=str(raw.get("patch_type", "")),
                strip=int(raw.get("patch_strip", 1)),
                base_path=str(raw.get("base_path", "")),
            )
        )
    return out
