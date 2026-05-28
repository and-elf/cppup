"""Fetch, hash, and optionally dry-run-apply `[[source.patches]]` entries.

The mapper composes patch URLs against `raw.githubusercontent.com`; this
module turns those URLs into verified `sha256` values (so the emitted
`package.toml` is actually checkable by the cppup client) and — under
`--verify-patches` — fetches the source tarball, extracts it, and runs
each patch through `patch-ng` in dry-run mode to catch broken patches
before they hit the registry.

`patch-ng` is the same library Conan uses internally, so any patch that
applies upstream applies here too. It is an optional dependency: import
is lazy and the verify path raises a clear error when the extra is not
installed.
"""

from __future__ import annotations

import hashlib
import io
import logging
import tarfile
import tempfile
import urllib.error
import urllib.request
import zipfile
from collections.abc import Callable, Iterable
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any

log = logging.getLogger(__name__)

DEFAULT_TIMEOUT = 30
DEFAULT_USER_AGENT = "conan2toml/0.1"


class PatchFetchError(RuntimeError):
    pass


class PatchVerifyError(RuntimeError):
    pass


@dataclass
class PatchVerifyResult:
    url: str
    applied: bool
    error: str = ""


@dataclass
class PatchEnrichReport:
    fetched: list[str] = field(default_factory=list)
    skipped: list[str] = field(default_factory=list)
    failed: list[tuple[str, str]] = field(default_factory=list)


Fetcher = Callable[[str], bytes]


def http_fetch(url: str, *, timeout: int = DEFAULT_TIMEOUT) -> bytes:
    req = urllib.request.Request(url, headers={"User-Agent": DEFAULT_USER_AGENT})
    try:
        with urllib.request.urlopen(req, timeout=timeout) as resp:
            return resp.read()
    except (urllib.error.URLError, TimeoutError) as exc:
        raise PatchFetchError(f"failed to fetch {url}: {exc}") from exc


def sha256_hex(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def enrich_patch_hashes(
    data: dict[str, Any],
    *,
    fetcher: Fetcher = http_fetch,
) -> PatchEnrichReport:
    """Fill in `sha256` for every `[[source.patches]]` entry in `data`.

    Skips entries whose `sha256` is already non-empty (idempotent). Each
    fetched patch's bytes are cached under `_data` on the entry so a
    subsequent verify pass does not re-download.
    """
    report = PatchEnrichReport()
    patches = (data.get("source") or {}).get("patches") or []
    for entry in patches:
        url = entry.get("url", "")
        if not url:
            continue
        if entry.get("sha256"):
            report.skipped.append(url)
            continue
        try:
            blob = fetcher(url)
        except PatchFetchError as exc:
            report.failed.append((url, str(exc)))
            continue
        entry["sha256"] = sha256_hex(blob)
        entry["_data"] = blob
        report.fetched.append(url)
    return report


def verify_patches(
    data: dict[str, Any],
    *,
    fetcher: Fetcher = http_fetch,
) -> list[PatchVerifyResult]:
    """Dry-run-apply every `[[source.patches]]` against the package source.

    Downloads `[source].url`, extracts it into a temp dir, then asks
    `patch-ng` to apply each patch. Returns one result per patch.

    Raises `PatchVerifyError` if the source archive cannot be fetched or
    extracted, or if `patch-ng` is not installed.
    """
    try:
        import patch_ng  # type: ignore[import-not-found]
    except ImportError as exc:
        raise PatchVerifyError(
            "patch-ng is required for --verify-patches "
            "(install with: uv pip install 'conan2toml[verify]')"
        ) from exc

    source = data.get("source") or {}
    patches = source.get("patches") or []
    if not patches:
        return []

    src_url = source.get("url")
    if not src_url:
        raise PatchVerifyError("source.url is missing; cannot verify patches")

    archive_bytes = fetcher(src_url)
    src_type = source.get("type", "tar")

    results: list[PatchVerifyResult] = []
    with tempfile.TemporaryDirectory(prefix="conan2toml-verify-") as tmp:
        tmp_path = Path(tmp)
        root = _extract_archive(archive_bytes, src_type, tmp_path)
        for entry in patches:
            url = entry.get("url", "")
            blob = entry.get("_data") or fetcher(url)
            strip = int(entry.get("strip", 1))
            try:
                ok, err = _try_apply(patch_ng, blob, root, strip)
            except Exception as exc:
                ok, err = False, f"{exc.__class__.__name__}: {exc}"
            results.append(PatchVerifyResult(url=url, applied=ok, error="" if ok else err))
    return results


def strip_internal_fields(data: dict[str, Any]) -> None:
    """Remove transient cache fields (`_data`) before emit."""
    patches = (data.get("source") or {}).get("patches") or []
    for entry in patches:
        entry.pop("_data", None)


def _extract_archive(blob: bytes, src_type: str, dest: Path) -> Path:
    """Extract `blob` into `dest`; return the path to the single top-level
    directory if there is one, else `dest` itself."""
    if src_type == "tar":
        with tarfile.open(fileobj=io.BytesIO(blob), mode="r:*") as tf:
            _safe_extract_tar(tf, dest)
    elif src_type == "zip":
        with zipfile.ZipFile(io.BytesIO(blob)) as zf:
            _safe_extract_zip(zf, dest)
    else:
        raise PatchVerifyError(
            f"cannot verify patches against source type {src_type!r} (only tar/zip supported)"
        )
    return _single_root(dest)


def _safe_extract_tar(tf: tarfile.TarFile, dest: Path) -> None:
    dest = dest.resolve()
    for member in tf.getmembers():
        target = (dest / member.name).resolve()
        if not _is_within(target, dest):
            raise PatchVerifyError(f"archive contains unsafe path: {member.name!r}")
    tf.extractall(dest)


def _safe_extract_zip(zf: zipfile.ZipFile, dest: Path) -> None:
    dest = dest.resolve()
    for name in zf.namelist():
        target = (dest / name).resolve()
        if not _is_within(target, dest):
            raise PatchVerifyError(f"archive contains unsafe path: {name!r}")
    zf.extractall(dest)


def _is_within(child: Path, parent: Path) -> bool:
    try:
        child.relative_to(parent)
        return True
    except ValueError:
        return False


def _single_root(dest: Path) -> Path:
    entries: Iterable[Path] = list(dest.iterdir())
    dirs = [p for p in entries if p.is_dir()]
    files = [p for p in entries if p.is_file()]
    if len(dirs) == 1 and not files:
        return dirs[0]
    return dest


def _try_apply(patch_ng, blob: bytes, root: Path, strip: int) -> tuple[bool, str]:
    """Apply `blob` against `root`. The root sits in a temp dir we are
    going to delete, so mutating it during the verify step is harmless —
    the success/failure of the apply is the whole signal we need.
    """
    pset = patch_ng.fromstring(blob)
    if not pset:
        return False, "patch-ng failed to parse the patch"
    ok = pset.apply(strip=_adjust_strip(blob, strip), root=str(root), fuzz=False)
    if not ok:
        return False, "patch-ng reported the patch did not apply"
    return True, ""


def _adjust_strip(blob: bytes, strip: int) -> int:
    """Translate a `patch -p<N>`-style strip value into the value
    `patch-ng` actually wants.

    `patch-ng.fromstring` auto-strips `a/` and `b/` prefixes during parse
    independently of the strip parameter. So a git-format patch with
    `--- a/include/foo.h` parses to `include/foo.h`, and `apply(strip=N)`
    then strips N *more* components. Conan's `patch_strip = 1` (the CCI
    default) counts the `a/` toward the strip count and would
    double-strip under patch-ng — convert by subtracting one when we
    spot the git prefix convention.
    """
    has_git_prefix = (
        b"\n--- a/" in blob
        or b"\n+++ b/" in blob
        or blob.startswith(b"--- a/")
        or blob.startswith(b"+++ b/")
    )
    return max(0, strip - 1) if has_git_prefix else strip
