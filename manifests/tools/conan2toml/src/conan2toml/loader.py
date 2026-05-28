"""Thin wrapper around Conan v2's recipe loader.

We use Conan's own loader rather than maintaining a mock SDK: the recipe
ecosystem keeps adding helpers (`tools.cmake.CMakeToolchain`,
`tools.scm.Git`, `tools.files.get`) and reimplementing those is a tax
that never stops. The trade-off is that running conan2toml requires
`conan>=2.5` to be importable in the same Python environment.

The loader returns a `ConanFile` instance whose class attributes (`name`,
`version`, `requires`, `options`, `default_options`, `package_type`) are
populated, plus `conan_data` resolved from the sibling `conandata.yml`.
Imperative dep declarations (`def requirements(self): self.requires(...)`)
are evaluated and lifted onto the instance as plain `requires` /
`tool_requires` / `test_requires` tuples so the mapper can iterate them
uniformly.
"""

from __future__ import annotations

import logging
from pathlib import Path

log = logging.getLogger(__name__)


class LoaderError(RuntimeError):
    pass


def load_conanfile(path: Path, *, version: str | None = None):
    """Load a conanfile.py and return a populated `ConanFile` instance.

    `version` is only consulted when the recipe omits a class-level
    `version` attribute (Conan Center recipes typically do). The chosen
    version selects which `conandata.yml` slice the loader resolves into
    `self.conan_data`.
    """
    try:
        from conan.internal.loader import load_python_file
    except ImportError as exc:
        raise LoaderError(
            "conan>=2.5 must be installed to load conanfile.py recipes "
            "(install with: uv pip install 'conan>=2.5,<3')"
        ) from exc

    path = Path(path)
    if not path.exists():
        raise LoaderError(f"conanfile not found: {path}")

    module, _ = load_python_file(str(path))
    cls = _find_conanfile_class(module)
    if cls is None:
        raise LoaderError(f"no ConanFile subclass found in {path}")

    instance = cls()
    if version is not None and not getattr(instance, "version", None):
        instance.version = version

    conandata_path = path.parent / "conandata.yml"
    if conandata_path.exists():
        import yaml

        instance.conan_data = yaml.safe_load(conandata_path.read_text()) or {}

    _resolve_imperative_requires(instance)
    return instance


def _resolve_imperative_requires(instance) -> None:
    """Invoke `requirements()` / `build_requirements()` and snapshot the
    resulting refs as plain tuples on the instance.

    Most Conan v2 recipes (spdlog, openssl, qt, ...) declare deps in the
    `requirements()` method rather than as a class-level `requires`
    tuple. Conan's `ConanFile.__init__` gives us a callable `Requirements`
    object that `self.requires(...)` appends to; after we invoke the
    method we lift the resulting list off as `tuple[str]` so the mapper
    sees the same shape whichever style the recipe used.
    """
    _wire_dep_helpers(instance)

    for method_name in ("requirements", "build_requirements"):
        method = getattr(type(instance), method_name, None)
        if not callable(method):
            continue
        try:
            method(instance)
        except Exception as exc:
            log.warning(
                "%s.%s() raised %s: %s — dep list may be incomplete",
                type(instance).__name__,
                method_name,
                exc.__class__.__name__,
                exc,
            )

    requires_obj = getattr(instance, "requires", None)
    if requires_obj is None or not hasattr(requires_obj, "values"):
        return  # already a plain tuple, or never populated

    runtime: list[str] = []
    build: list[str] = []
    test: list[str] = []
    for req in requires_obj.values():
        ref = str(getattr(req, "ref", "")).strip()
        if not ref:
            continue
        if _truthy(getattr(req, "_build", None)) or _truthy(getattr(req, "build", None)):
            build.append(ref)
        elif _truthy(getattr(req, "_test", None)) or _truthy(getattr(req, "is_test", None)):
            test.append(ref)
        else:
            runtime.append(ref)

    instance.requires = tuple(runtime)
    # Conan v2 distinguishes `build_requires` (legacy) from `tool_requires`
    # (current); both end up in the same Requirements bucket with
    # `_build=True`. Expose under both attrs so mapper rules that check
    # either source match.
    instance.tool_requires = tuple(build)
    instance.build_requires = tuple(build)
    instance.test_requires = tuple(test)


def _truthy(value) -> bool:
    return bool(value) and value is not None


def _wire_dep_helpers(instance) -> None:
    """Make `self.tool_requires(...)` / `self.test_requires(...)` callable.

    Conan only binds these as bound methods during full graph loading;
    after a bare `cls()` they are `None`, so any recipe that calls
    `self.tool_requires("cmake/...")` in `build_requirements()` crashes
    with `'NoneType' object is not callable`. Delegate to the matching
    `Requirements` method so the recipe can declare the dep with the
    right `_build` / `_test` flag.
    """
    reqs = getattr(instance, "requires", None)
    if reqs is None or not hasattr(reqs, "tool_require"):
        return
    if getattr(instance, "tool_requires", None) is None:
        instance.tool_requires = reqs.tool_require
    if getattr(instance, "test_requires", None) is None:
        instance.test_requires = reqs.test_require
    if getattr(instance, "build_requires", None) is None:
        instance.build_requires = reqs.build_require


def _find_conanfile_class(module):
    try:
        from conan import ConanFile
    except ImportError as exc:
        raise LoaderError("conan>=2.5 must be installed") from exc

    for name in dir(module):
        obj = getattr(module, name)
        if not isinstance(obj, type):
            continue
        if obj is ConanFile:
            continue
        try:
            if issubclass(obj, ConanFile):
                return obj
        except TypeError:
            continue
    return None
