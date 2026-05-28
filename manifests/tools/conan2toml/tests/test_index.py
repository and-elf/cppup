from __future__ import annotations

from pathlib import Path

import yaml

from conan2toml.index import upsert_version


def write_index(tmp_path: Path, body: str) -> Path:
    p = tmp_path / "index.yaml"
    p.write_text(body)
    return p


def test_upsert_version_creates_package_entry_when_missing(tmp_path: Path):
    p = write_index(tmp_path, "schema: 1\npackages: {}\n")
    upsert_version(
        p,
        name="fmt",
        version="10.2.1",
        manifest_relpath="recipes/fmt/10.2.1/package.toml",
        manifest_sha256="deadbeef",
        source_url="https://github.com/fmtlib/fmt.git",
        source_ref="10.2.1",
        kind="library",
        description="A modern formatting library",
        license_name="MIT",
        homepage="https://fmt.dev",
        repository="https://github.com/fmtlib/fmt",
    )
    data = yaml.safe_load(p.read_text())
    pkg = data["packages"]["fmt"]
    assert pkg["kind"] == "library"
    assert pkg["latest"] == "10.2.1"
    assert pkg["versions"]["10.2.1"]["manifest_sha256"] == "deadbeef"


def test_upsert_version_replaces_existing_version_entry(tmp_path: Path):
    p = write_index(
        tmp_path,
        """
schema: 1
packages:
  fmt:
    kind: library
    latest: "10.2.1"
    versions:
      "10.2.1":
        manifest: old
        manifest_sha256: stale
""",
    )
    upsert_version(
        p,
        name="fmt",
        version="10.2.1",
        manifest_relpath="recipes/fmt/10.2.1/package.toml",
        manifest_sha256="fresh",
        source_url="u",
        source_ref="10.2.1",
        kind="library",
    )
    data = yaml.safe_load(p.read_text())
    assert data["packages"]["fmt"]["versions"]["10.2.1"]["manifest_sha256"] == "fresh"


def test_upsert_version_keeps_existing_other_versions(tmp_path: Path):
    p = write_index(
        tmp_path,
        """
schema: 1
packages:
  fmt:
    kind: library
    latest: "10.1.1"
    versions:
      "10.1.1":
        manifest: recipes/fmt/10.1.1/package.toml
        manifest_sha256: oldsum
""",
    )
    upsert_version(
        p,
        name="fmt",
        version="10.2.1",
        manifest_relpath="recipes/fmt/10.2.1/package.toml",
        manifest_sha256="newsum",
        source_url="u",
        source_ref="10.2.1",
        kind="library",
    )
    data = yaml.safe_load(p.read_text())
    vers = data["packages"]["fmt"]["versions"]
    assert "10.1.1" in vers
    assert "10.2.1" in vers


def test_upsert_version_picks_highest_as_latest(tmp_path: Path):
    p = write_index(
        tmp_path,
        """
schema: 1
packages:
  fmt:
    kind: library
    latest: "10.1.1"
    versions:
      "10.1.1": { manifest: x, manifest_sha256: s }
""",
    )
    upsert_version(
        p,
        name="fmt",
        version="10.2.1",
        manifest_relpath="recipes/fmt/10.2.1/package.toml",
        manifest_sha256="s",
        source_url="u",
        source_ref="10.2.1",
        kind="library",
    )
    data = yaml.safe_load(p.read_text())
    assert data["packages"]["fmt"]["latest"] == "10.2.1"


def test_upsert_version_does_not_demote_latest_when_older_version_added(tmp_path: Path):
    p = write_index(
        tmp_path,
        """
schema: 1
packages:
  fmt:
    kind: library
    latest: "10.2.1"
    versions:
      "10.2.1": { manifest: x, manifest_sha256: s }
""",
    )
    upsert_version(
        p,
        name="fmt",
        version="10.1.1",
        manifest_relpath="recipes/fmt/10.1.1/package.toml",
        manifest_sha256="s",
        source_url="u",
        source_ref="10.1.1",
        kind="library",
    )
    data = yaml.safe_load(p.read_text())
    assert data["packages"]["fmt"]["latest"] == "10.2.1"
