from __future__ import annotations

import hashlib
import io
import tarfile
from pathlib import Path

import pytest

from conan2toml.patches import (
    PatchVerifyError,
    _adjust_strip,
    enrich_patch_hashes,
    sha256_hex,
    strip_internal_fields,
    verify_patches,
)


def _manifest(*, patches: list[dict], source_url: str = "https://x/src.tar.gz") -> dict:
    return {
        "source": {
            "type": "tar",
            "url": source_url,
            "checksum": "sha256:00",
            "patches": patches,
        },
    }


def test_sha256_hex_matches_hashlib():
    assert sha256_hex(b"abc") == hashlib.sha256(b"abc").hexdigest()


def test_enrich_fills_empty_sha256_from_fetcher():
    data = _manifest(
        patches=[
            {"url": "https://x/a.patch", "sha256": "", "description": "", "strip": 1},
            {"url": "https://x/b.patch", "sha256": "", "description": "", "strip": 1},
        ]
    )
    payloads = {"https://x/a.patch": b"PATCH-A", "https://x/b.patch": b"PATCH-B"}
    report = enrich_patch_hashes(data, fetcher=lambda u: payloads[u])

    patches = data["source"]["patches"]
    assert patches[0]["sha256"] == sha256_hex(b"PATCH-A")
    assert patches[1]["sha256"] == sha256_hex(b"PATCH-B")
    assert report.fetched == ["https://x/a.patch", "https://x/b.patch"]
    assert report.skipped == []


def test_enrich_skips_already_hashed_entries():
    data = _manifest(
        patches=[
            {"url": "https://x/a.patch", "sha256": "deadbeef", "description": "", "strip": 1},
        ]
    )

    def fetcher(_url: str) -> bytes:
        raise AssertionError("fetcher must not be called for pre-hashed entries")

    report = enrich_patch_hashes(data, fetcher=fetcher)
    assert data["source"]["patches"][0]["sha256"] == "deadbeef"
    assert report.skipped == ["https://x/a.patch"]
    assert report.fetched == []


def test_enrich_records_fetch_failures_without_raising():
    from conan2toml.patches import PatchFetchError

    data = _manifest(
        patches=[
            {"url": "https://x/a.patch", "sha256": "", "description": "", "strip": 1},
        ]
    )

    def fetcher(_url: str) -> bytes:
        raise PatchFetchError("offline")

    report = enrich_patch_hashes(data, fetcher=fetcher)
    assert data["source"]["patches"][0]["sha256"] == ""
    assert report.failed == [("https://x/a.patch", "offline")]


def test_strip_internal_fields_removes_cached_data():
    data = _manifest(
        patches=[
            {"url": "u", "sha256": "h", "_data": b"raw", "description": "", "strip": 1},
        ]
    )
    strip_internal_fields(data)
    assert "_data" not in data["source"]["patches"][0]


def _make_tar(files: dict[str, bytes]) -> bytes:
    buf = io.BytesIO()
    with tarfile.open(fileobj=buf, mode="w:gz") as tf:
        for name, payload in files.items():
            info = tarfile.TarInfo(name)
            info.size = len(payload)
            tf.addfile(info, io.BytesIO(payload))
    return buf.getvalue()


def _patch_for(filename: str, before: bytes, after: bytes) -> bytes:
    """Construct a minimal unified diff converting `before` to `after`.

    Only handles the one-line-replacement shape we use in tests; that is
    enough to exercise patch-ng end-to-end.
    """
    return (
        f"--- a/{filename}\n"
        f"+++ b/{filename}\n"
        "@@ -1 +1 @@\n"
        f"-{before.decode()}"
        f"+{after.decode()}"
    ).encode()


def test_verify_patches_applies_clean_patch(tmp_path: Path):
    pytest.importorskip("patch_ng")

    src_bytes = _make_tar({"pkg-1.0/hello.txt": b"hello\n"})
    patch_bytes = _patch_for("hello.txt", b"hello\n", b"world\n")

    fetched: dict[str, bytes] = {
        "https://x/src.tar.gz": src_bytes,
        "https://x/fix.patch": patch_bytes,
    }
    data = _manifest(
        patches=[
            {"url": "https://x/fix.patch", "sha256": "", "description": "", "strip": 1},
        ]
    )

    results = verify_patches(data, fetcher=lambda u: fetched[u])
    assert len(results) == 1
    assert results[0].applied is True, results[0].error


def test_verify_patches_reports_broken_patch():
    pytest.importorskip("patch_ng")

    src_bytes = _make_tar({"pkg-1.0/hello.txt": b"different content\n"})
    patch_bytes = _patch_for("hello.txt", b"hello\n", b"world\n")

    fetched = {
        "https://x/src.tar.gz": src_bytes,
        "https://x/fix.patch": patch_bytes,
    }
    data = _manifest(
        patches=[
            {"url": "https://x/fix.patch", "sha256": "", "description": "", "strip": 1},
        ]
    )

    results = verify_patches(data, fetcher=lambda u: fetched[u])
    assert len(results) == 1
    assert results[0].applied is False
    assert results[0].error


def test_verify_patches_empty_when_no_patches():
    data = _manifest(patches=[])
    assert verify_patches(data, fetcher=lambda _u: b"") == []


def test_adjust_strip_subtracts_one_for_git_style_patches():
    git_patch = (
        b"diff --git a/foo b/foo\n"
        b"--- a/foo\n"
        b"+++ b/foo\n"
        b"@@ -1 +1 @@\n"
        b"-old\n"
        b"+new\n"
    )
    assert _adjust_strip(git_patch, strip=1) == 0
    assert _adjust_strip(git_patch, strip=2) == 1
    # patch_strip=0 doesn't go negative.
    assert _adjust_strip(git_patch, strip=0) == 0


def test_adjust_strip_passes_through_for_bare_patches():
    bare_patch = (
        b"--- foo\n"
        b"+++ foo\n"
        b"@@ -1 +1 @@\n"
        b"-old\n"
        b"+new\n"
    )
    assert _adjust_strip(bare_patch, strip=1) == 1
    assert _adjust_strip(bare_patch, strip=0) == 0


def test_verify_patches_rejects_unsupported_source_type():
    pytest.importorskip("patch_ng")

    data = {
        "source": {
            "type": "git",
            "url": "https://x/repo.git",
            "checksum": "",
            "patches": [
                {"url": "https://x/fix.patch", "sha256": "", "description": "", "strip": 0},
            ],
        },
    }
    with pytest.raises(PatchVerifyError):
        verify_patches(data, fetcher=lambda _u: b"junk")
