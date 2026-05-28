from __future__ import annotations

from types import SimpleNamespace

import pytest

from conan2toml.conandata import PatchEntry, SourceEntry
from conan2toml.mapper import RecipeSnapshot, map_recipe


def fake_conanfile(**overrides) -> SimpleNamespace:
    base = {
        "name": "fmt",
        "version": "10.2.1",
        "license": "MIT",
        "homepage": "https://fmt.dev",
        "url": "https://github.com/conan-io/conan-center-index",
        "description": "A modern formatting library",
        "topics": ("format",),
        "package_type": "library",
        "options": {
            "shared": [True, False],
            "fPIC": [True, False],
            "header_only": [True, False],
        },
        "default_options": {"shared": False, "fPIC": True, "header_only": False},
        "requires": (),
        "tool_requires": (),
        "test_requires": (),
        "settings": ("os", "arch", "compiler", "build_type"),
    }
    base.update(overrides)
    return SimpleNamespace(**base)


def test_map_recipe_populates_package_metadata():
    snap = RecipeSnapshot(
        conanfile=fake_conanfile(),
        source=SourceEntry(url="https://github.com/fmtlib/fmt/archive/10.2.1.tar.gz", sha256="abc"),
        patches=[],
    )
    out = map_recipe(snap)
    assert out["package"]["name"] == "fmt"
    assert out["package"]["version"] == "10.2.1"
    assert out["package"]["kind"] == "library"
    assert out["package"]["license"] == "MIT"
    assert out["package"]["homepage"] == "https://fmt.dev"
    assert out["package"]["description"] == "A modern formatting library"


def test_map_recipe_maps_conan_url_to_repository():
    snap = RecipeSnapshot(
        conanfile=fake_conanfile(url="https://github.com/conan-io/conan-center-index"),
        source=SourceEntry(url="https://x/x.tar.gz", sha256="abc"),
        patches=[],
    )
    out = map_recipe(snap)
    assert out["package"]["repository"] == "https://github.com/conan-io/conan-center-index"


def test_map_recipe_emits_schema_version():
    snap = RecipeSnapshot(
        conanfile=fake_conanfile(), source=SourceEntry(url="u", sha256="s"), patches=[]
    )
    out = map_recipe(snap)
    assert out["schema"] == 1


def test_map_recipe_source_is_tar_when_url_ends_in_archive():
    snap = RecipeSnapshot(
        conanfile=fake_conanfile(),
        source=SourceEntry(url="https://x/foo.tar.gz", sha256="abcd"),
        patches=[],
    )
    out = map_recipe(snap)
    assert out["source"]["type"] == "tar"
    assert out["source"]["url"] == "https://x/foo.tar.gz"
    assert out["source"]["checksum"] == "sha256:abcd"


def test_map_recipe_source_is_zip_when_url_ends_in_zip():
    snap = RecipeSnapshot(
        conanfile=fake_conanfile(),
        source=SourceEntry(url="https://x/foo.zip", sha256="abcd"),
        patches=[],
    )
    out = map_recipe(snap)
    assert out["source"]["type"] == "zip"


def test_map_recipe_features_default_from_default_options():
    snap = RecipeSnapshot(
        conanfile=fake_conanfile(), source=SourceEntry(url="u", sha256="s"), patches=[]
    )
    out = map_recipe(snap)
    assert out["features"]["shared"]["default"] is False
    assert out["features"]["fPIC"]["default"] is True
    assert out["features"]["header_only"]["default"] is False


def test_map_recipe_emits_known_descriptions_for_canonical_options():
    snap = RecipeSnapshot(
        conanfile=fake_conanfile(), source=SourceEntry(url="u", sha256="s"), patches=[]
    )
    out = map_recipe(snap)
    assert out["features"]["shared"]["description"] == "Build as shared library"
    assert out["features"]["fPIC"]["description"] == "Position-independent code"
    assert out["features"]["header_only"]["description"] == "Header-only mode"


def test_map_recipe_falls_back_description_for_unknown_options():
    snap = RecipeSnapshot(
        conanfile=fake_conanfile(
            options={"with_foo": [True, False]}, default_options={"with_foo": True}
        ),
        source=SourceEntry(url="u", sha256="s"),
        patches=[],
    )
    out = map_recipe(snap)
    assert out["features"]["with_foo"]["description"] == "Conan option: with_foo"


def test_map_recipe_omits_features_when_no_options():
    snap = RecipeSnapshot(
        conanfile=fake_conanfile(options={}, default_options={}),
        source=SourceEntry(url="u", sha256="s"),
        patches=[],
    )
    out = map_recipe(snap)
    assert "features" not in out


def test_map_recipe_dependencies_from_requires_tuple():
    snap = RecipeSnapshot(
        conanfile=fake_conanfile(requires=("zlib/1.3.1", "openssl/3.2.0")),
        source=SourceEntry(url="u", sha256="s"),
        patches=[],
    )
    out = map_recipe(snap)
    assert out["dependencies"]["zlib"] == "=1.3.1"
    assert out["dependencies"]["openssl"] == "=3.2.0"


def test_map_recipe_build_dependencies_from_tool_requires():
    snap = RecipeSnapshot(
        conanfile=fake_conanfile(tool_requires=("cmake/3.27.0",)),
        source=SourceEntry(url="u", sha256="s"),
        patches=[],
    )
    out = map_recipe(snap)
    assert out["build-dependencies"]["cmake"] == "=3.27.0"


def test_map_recipe_test_dependencies_from_test_requires():
    snap = RecipeSnapshot(
        conanfile=fake_conanfile(test_requires=("gtest/1.14.0",)),
        source=SourceEntry(url="u", sha256="s"),
        patches=[],
    )
    out = map_recipe(snap)
    assert out["test-dependencies"]["gtest"] == "=1.14.0"


def test_map_recipe_patches_become_source_patches():
    snap = RecipeSnapshot(
        conanfile=fake_conanfile(),
        source=SourceEntry(url="u", sha256="s"),
        patches=[PatchEntry(file="patches/fix.patch", description="Fix", type="portability")],
    )
    out = map_recipe(snap, recipe_repo_path="recipes/fmt/all", cci_commit="DEADBEEF")
    patches = out["source"]["patches"]
    assert len(patches) == 1
    assert (
        patches[0]["url"]
        == "https://raw.githubusercontent.com/conan-io/conan-center-index/DEADBEEF/recipes/fmt/all/patches/fix.patch"
    )
    assert patches[0]["description"] == "Fix"
    assert patches[0]["strip"] == 1


def test_map_recipe_kind_defaults_to_library():
    snap = RecipeSnapshot(
        conanfile=fake_conanfile(package_type=None),
        source=SourceEntry(url="u", sha256="s"),
        patches=[],
    )
    out = map_recipe(snap)
    assert out["package"]["kind"] == "library"


@pytest.mark.parametrize(
    "package_type,kind",
    [("library", "library"), ("application", "tool"), ("shared-library", "library")],
)
def test_map_recipe_kind_mapping(package_type, kind):
    snap = RecipeSnapshot(
        conanfile=fake_conanfile(package_type=package_type),
        source=SourceEntry(url="u", sha256="s"),
        patches=[],
    )
    out = map_recipe(snap)
    assert out["package"]["kind"] == kind


def test_map_recipe_collects_todos_for_imperative_methods():
    cf = fake_conanfile()

    def build(self):
        ...

    def package_info(self):
        ...

    cf.build = build
    cf.package_info = package_info
    snap = RecipeSnapshot(conanfile=cf, source=SourceEntry(url="u", sha256="s"), patches=[])
    out = map_recipe(snap)
    todos = out.get("_todos", [])
    assert any("build()" in t for t in todos)
    assert any("package_info()" in t for t in todos)
