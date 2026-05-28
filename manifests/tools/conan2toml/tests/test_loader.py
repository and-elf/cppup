"""Loader behavior — particularly the imperative-requires lift.

Most CCI recipes (spdlog, openssl, qt, ...) declare deps inside a
`def requirements(self)` method rather than as a class-level
`requires = (...)` tuple. The loader needs to invoke that method and
surface the resulting refs as plain attributes so the mapper sees the
same shape regardless of recipe style.
"""

from __future__ import annotations

from pathlib import Path
from textwrap import dedent

import pytest

pytest.importorskip("conan")

from conan2toml.loader import load_conanfile


def _write_recipe(tmp_path: Path, body: str) -> Path:
    p = tmp_path / "conanfile.py"
    p.write_text(dedent(body))
    return p


def test_loader_lifts_runtime_requires_from_requirements_method(tmp_path: Path):
    recipe = _write_recipe(
        tmp_path,
        """
        from conan import ConanFile

        class Demo(ConanFile):
            name = "demo"
            version = "1.0"
            def requirements(self):
                self.requires("fmt/10.2.1")
                self.requires("zlib/1.3.1")
        """,
    )
    cf = load_conanfile(recipe)
    assert "fmt/10.2.1" in cf.requires
    assert "zlib/1.3.1" in cf.requires
    assert cf.tool_requires == ()
    assert cf.test_requires == ()


def test_loader_classifies_tool_requires_as_build_deps(tmp_path: Path):
    recipe = _write_recipe(
        tmp_path,
        """
        from conan import ConanFile

        class Demo(ConanFile):
            name = "demo"
            version = "1.0"
            def build_requirements(self):
                self.tool_requires("cmake/3.27.0")
        """,
    )
    cf = load_conanfile(recipe)
    assert cf.requires == ()
    assert "cmake/3.27.0" in cf.tool_requires
    assert "cmake/3.27.0" in cf.build_requires


def test_loader_classifies_test_requires(tmp_path: Path):
    recipe = _write_recipe(
        tmp_path,
        """
        from conan import ConanFile

        class Demo(ConanFile):
            name = "demo"
            version = "1.0"
            def build_requirements(self):
                self.test_requires("gtest/1.14.0")
        """,
    )
    cf = load_conanfile(recipe)
    assert cf.requires == ()
    assert "gtest/1.14.0" in cf.test_requires


def test_loader_passes_version_to_requirements(tmp_path: Path):
    """The `--version` CLI flag must reach `self.version` *before*
    `requirements()` runs — spdlog's `requirements()` reads
    `self.conan_data["fmt_version_mapping"][self.version]`.
    """
    recipe = _write_recipe(
        tmp_path,
        """
        from conan import ConanFile

        class Demo(ConanFile):
            name = "demo"
            def requirements(self):
                self.requires(f"fmt/{self.version}")
        """,
    )
    cf = load_conanfile(recipe, version="9.9.9")
    assert "fmt/9.9.9" in cf.requires


def test_loader_tolerates_requirements_raising(tmp_path: Path):
    """A recipe whose `requirements()` references conan_data we don't
    have should not abort conversion — the warning is logged, and the
    rest of the recipe still translates.
    """
    recipe = _write_recipe(
        tmp_path,
        """
        from conan import ConanFile

        class Demo(ConanFile):
            name = "demo"
            version = "1.0"
            def requirements(self):
                # Missing conan_data key — this raises KeyError.
                self.requires(f"x/{self.conan_data['missing_key']}")
        """,
    )
    cf = load_conanfile(recipe)
    assert cf.name == "demo"
    assert cf.requires == ()


def test_loader_leaves_class_requires_alone_when_no_method(tmp_path: Path):
    recipe = _write_recipe(
        tmp_path,
        """
        from conan import ConanFile

        class Demo(ConanFile):
            name = "demo"
            version = "1.0"
            requires = ("fmt/10.2.1",)
        """,
    )
    cf = load_conanfile(recipe)
    # No requirements() method; Conan still builds a Requirements obj
    # from the class attr — lifted to a plain tuple regardless.
    assert "fmt/10.2.1" in cf.requires
