from __future__ import annotations

from pathlib import Path

import pytest

from conan2toml.conandata import (
    ConandataError,
    PatchEntry,
    SourceEntry,
    load_conandata,
    source_for_version,
)


def write_conandata(tmp_path: Path, body: str) -> Path:
    path = tmp_path / "conandata.yml"
    path.write_text(body)
    return path


def test_load_conandata_returns_parsed_dict(tmp_path: Path):
    p = write_conandata(
        tmp_path,
        """
sources:
  "1.0":
    url: "https://x/1.0.tar.gz"
    sha256: "abc"
""",
    )
    data = load_conandata(p)
    assert data["sources"]["1.0"]["url"] == "https://x/1.0.tar.gz"


def test_source_for_version_picks_single_url(tmp_path: Path):
    p = write_conandata(
        tmp_path,
        """
sources:
  "1.0":
    url: "https://x/1.0.tar.gz"
    sha256: "abcdef"
""",
    )
    data = load_conandata(p)
    src = source_for_version(data, "1.0")
    assert isinstance(src, SourceEntry)
    assert src.url == "https://x/1.0.tar.gz"
    assert src.sha256 == "abcdef"


def test_source_for_version_handles_url_list(tmp_path: Path):
    p = write_conandata(
        tmp_path,
        """
sources:
  "1.0":
    url:
      - "https://primary/1.0.tar.gz"
      - "https://mirror/1.0.tar.gz"
    sha256: "abcdef"
""",
    )
    data = load_conandata(p)
    src = source_for_version(data, "1.0")
    assert src.url == "https://primary/1.0.tar.gz"


def test_source_for_version_missing_raises(tmp_path: Path):
    p = write_conandata(
        tmp_path,
        """
sources:
  "1.0":
    url: "u"
    sha256: "s"
""",
    )
    data = load_conandata(p)
    with pytest.raises(ConandataError):
        source_for_version(data, "2.0")


def test_source_for_version_missing_sha_raises(tmp_path: Path):
    p = write_conandata(
        tmp_path,
        """
sources:
  "1.0":
    url: "u"
""",
    )
    data = load_conandata(p)
    with pytest.raises(ConandataError):
        source_for_version(data, "1.0")


def test_patches_for_version_returns_typed_entries(tmp_path: Path):
    from conan2toml.conandata import patches_for_version

    p = write_conandata(
        tmp_path,
        """
sources:
  "1.0":
    url: "u"
    sha256: "s"
patches:
  "1.0":
    - patch_file: "patches/fix.patch"
      patch_description: "Fix something"
      patch_type: "portability"
""",
    )
    data = load_conandata(p)
    patches = patches_for_version(data, "1.0")
    assert len(patches) == 1
    assert isinstance(patches[0], PatchEntry)
    assert patches[0].file == "patches/fix.patch"
    assert patches[0].description == "Fix something"


def test_patches_for_version_empty_when_absent(tmp_path: Path):
    from conan2toml.conandata import patches_for_version

    p = write_conandata(
        tmp_path,
        """
sources:
  "1.0": { url: "u", sha256: "s" }
""",
    )
    data = load_conandata(p)
    assert patches_for_version(data, "1.0") == []


def test_load_conandata_missing_file_raises(tmp_path: Path):
    with pytest.raises(ConandataError):
        load_conandata(tmp_path / "missing.yml")
