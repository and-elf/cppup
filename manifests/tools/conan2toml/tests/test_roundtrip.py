"""End-to-end loader → mapper → emitter test.

Requires conan>=2.5 to be importable; skipped otherwise. The fixture under
tests/fixtures/fmt/ is a trimmed copy of the real conan-center-index fmt
recipe pinned to 10.2.1.
"""

from __future__ import annotations

from pathlib import Path

import pytest

conan = pytest.importorskip("conan")

from conan2toml.conandata import (  # noqa: E402
    load_conandata,
    patches_for_version,
    source_for_version,
)
from conan2toml.emitter import emit_toml  # noqa: E402
from conan2toml.loader import load_conanfile  # noqa: E402
from conan2toml.mapper import RecipeSnapshot, map_recipe  # noqa: E402


def test_fmt_fixture_round_trip(fmt_fixture: Path):
    cf = load_conanfile(fmt_fixture / "conanfile.py", version="10.2.1")
    data = load_conandata(fmt_fixture / "conandata.yml")
    snap = RecipeSnapshot(
        conanfile=cf,
        source=source_for_version(data, "10.2.1"),
        patches=patches_for_version(data, "10.2.1"),
    )
    out = emit_toml(map_recipe(snap))
    expected = (fmt_fixture / "expected.toml").read_text()
    assert out == expected
