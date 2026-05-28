"""Map a loaded Conan recipe snapshot into a cppup package.toml schema dict.

Inputs: a `ConanFile`-like object (anything that exposes class attributes
like `name`, `version`, `requires`, ...), plus the per-version
`conandata.yml` slice (source + patches) the loader already resolved.

Output: a dict the emitter can render directly. The schema mirrors
`manifests/README.md`.
"""

from __future__ import annotations

from dataclasses import dataclass
from typing import Any, Protocol

from .conandata import PatchEntry, SourceEntry

DEFAULT_CPPUP_COMPAT = ">=0.5.0"
DEFAULT_PLATFORM_OS: list[str] = ["linux", "macos", "windows"]
DEFAULT_PLATFORM_ARCH: list[str] = ["x86_64", "arm64"]
CCI_RAW_BASE = "https://raw.githubusercontent.com/conan-io/conan-center-index"

_KNOWN_OPTION_DESCRIPTIONS: dict[str, str] = {
    "shared": "Build as shared library",
    "fPIC": "Position-independent code",
    "header_only": "Header-only mode",
}

_PACKAGE_TYPE_TO_KIND: dict[str, str] = {
    "library": "library",
    "shared-library": "library",
    "static-library": "library",
    "header-library": "library",
    "application": "tool",
    "build-scripts": "tool",
    "python-require": "plugin",
    "unknown": "library",
}


class ConanFileLike(Protocol):
    name: str
    version: str


@dataclass
class RecipeSnapshot:
    conanfile: Any
    source: SourceEntry
    patches: list[PatchEntry]


def map_recipe(
    snap: RecipeSnapshot,
    *,
    recipe_repo_path: str | None = None,
    cci_commit: str | None = None,
) -> dict[str, Any]:
    cf = snap.conanfile
    out: dict[str, Any] = {
        "schema": 1,
        "package": _map_package(cf),
        "source": _map_source(snap.source, snap.patches, recipe_repo_path, cci_commit),
        "build": _map_build(cf),
    }
    features = _map_features(cf)
    if features:
        out["features"] = features
    deps = _map_dep_block(getattr(cf, "requires", ()))
    if deps:
        out["dependencies"] = deps
    bdeps = _map_dep_block(getattr(cf, "tool_requires", ())) or _map_dep_block(
        getattr(cf, "build_requires", ())
    )
    if bdeps:
        out["build-dependencies"] = bdeps
    tdeps = _map_dep_block(getattr(cf, "test_requires", ()))
    if tdeps:
        out["test-dependencies"] = tdeps
    out["platforms"] = _map_platforms(cf)
    todos = _collect_todos(cf)
    if todos:
        out["_todos"] = todos
    return out


def _map_package(cf: Any) -> dict[str, Any]:
    name = getattr(cf, "name", None)
    version = getattr(cf, "version", None)
    if not name or not version:
        raise ValueError("conanfile is missing name or version")
    pkg: dict[str, Any] = {
        "name": str(name),
        "version": str(version),
        "kind": _map_kind(getattr(cf, "package_type", None)),
    }
    for src_attr, dst_key in (
        ("description", "description"),
        ("license", "license"),
        ("homepage", "homepage"),
        ("url", "repository"),
    ):
        val = getattr(cf, src_attr, None)
        if val:
            pkg[dst_key] = str(val)
    pkg["cppup_compat"] = DEFAULT_CPPUP_COMPAT
    return pkg


def _map_kind(package_type: Any) -> str:
    if not package_type:
        return "library"
    return _PACKAGE_TYPE_TO_KIND.get(str(package_type), "library")


def _map_source(
    src: SourceEntry,
    patches: list[PatchEntry],
    recipe_repo_path: str | None,
    cci_commit: str | None,
) -> dict[str, Any]:
    body: dict[str, Any] = {
        "type": _source_type_from_url(src.url),
        "url": src.url,
        "checksum": f"sha256:{src.sha256}",
    }
    rendered_patches = _map_patches(patches, recipe_repo_path, cci_commit)
    if rendered_patches:
        body["patches"] = rendered_patches
    return body


def _source_type_from_url(url: str) -> str:
    lower = url.lower()
    if lower.endswith(".zip"):
        return "zip"
    if lower.endswith(".git"):
        return "git"
    return "tar"


def _map_patches(
    patches: list[PatchEntry], recipe_repo_path: str | None, cci_commit: str | None
) -> list[dict[str, Any]]:
    if not patches or not recipe_repo_path or not cci_commit:
        return []
    out: list[dict[str, Any]] = []
    for p in patches:
        out.append(
            {
                "url": f"{CCI_RAW_BASE}/{cci_commit}/{recipe_repo_path}/{p.file}",
                "sha256": "",
                "description": p.description,
                "strip": p.strip,
            }
        )
    return out


def _map_build(cf: Any) -> dict[str, Any]:
    generators = getattr(cf, "generators", None) or ()
    if isinstance(generators, str):
        generators = (generators,)
    system = "meson" if any("Meson" in str(g) for g in generators) else "cmake"
    return {"system": system, "args": []}


def _map_features(cf: Any) -> dict[str, Any]:
    options = _option_schema(cf)
    defaults = _default_options(cf)
    if not options:
        return {}
    out: dict[str, Any] = {}
    for opt_name, opt_values in options.items():
        if _is_bool_option(opt_values):
            default = bool(defaults.get(opt_name, False))
            out[opt_name] = {
                "default": default,
                "description": _describe_option(opt_name),
            }
        else:
            values = [str(v) for v in opt_values if v != "ANY"]
            if not values:
                continue
            default = str(defaults.get(opt_name, values[0]))
            out[opt_name] = {
                "type": "enum",
                "values": values,
                "default": default,
                "description": _describe_option(opt_name),
            }
    return out


def _is_bool_option(values: Any) -> bool:
    if not isinstance(values, (list, tuple)):
        return False
    return set(values) == {True, False}


def _option_schema(cf: Any) -> dict[str, Any]:
    """Return `{opt_name: [allowed_values]}` regardless of recipe shape.

    Plain test namespaces expose the schema dict directly on the instance.
    Conan-loaded `ConanFile` instances shadow it with an `Options` resolver
    that yields current values — the schema only survives on the class.
    """
    inst = getattr(cf, "options", None)
    if isinstance(inst, dict):
        return inst
    cls_opts = getattr(type(cf), "options", None)
    if isinstance(cls_opts, dict):
        return cls_opts
    return {}


def _default_options(cf: Any) -> dict[str, Any]:
    inst = getattr(cf, "default_options", None)
    if isinstance(inst, dict):
        return inst
    cls_defs = getattr(type(cf), "default_options", None)
    if isinstance(cls_defs, dict):
        return cls_defs
    return {}


def _describe_option(name: str) -> str:
    return _KNOWN_OPTION_DESCRIPTIONS.get(name, f"Conan option: {name}")


def _map_dep_block(refs: Any) -> dict[str, str]:
    if not refs:
        return {}
    if isinstance(refs, str):
        refs = (refs,)
    out: dict[str, str] = {}
    for ref in refs:
        if not isinstance(ref, str) or "/" not in ref:
            continue
        name, _, rest = ref.partition("/")
        constraint = rest.split("@", 1)[0].strip()
        if not constraint:
            continue
        out[name] = constraint if any(c in constraint for c in "<>=~^") else f"={constraint}"
    return out


def _map_platforms(cf: Any) -> dict[str, list[str]]:
    settings = getattr(cf, "settings", None) or ()
    if not isinstance(settings, (str, list, tuple)):
        settings = ()
    out: dict[str, list[str]] = {}
    if "os" in settings:
        out["os"] = list(DEFAULT_PLATFORM_OS)
    if "arch" in settings:
        out["arch"] = list(DEFAULT_PLATFORM_ARCH)
    if not out:
        out["os"] = list(DEFAULT_PLATFORM_OS)
    return out


def _collect_todos(cf: Any) -> list[str]:
    method_hints = {
        "build": "build(): custom build steps - map to a cppup build plugin",
        "package": "package(): custom install/copy steps - typically covered by CMake install",
        "package_info": "package_info(): cpp_info populated imperatively - verify [[exports]]",
        "validate": "validate(): refuses unsupported settings - encode as [platforms] constraint",
    }
    todos: list[str] = []
    for method, hint in method_hints.items():
        attr = getattr(cf, method, None)
        if callable(attr) and not _is_inherited_noop(attr, method):
            todos.append(hint)
    return todos


def _is_inherited_noop(attr: Any, method: str) -> bool:
    qual = getattr(attr, "__qualname__", "")
    return qual.startswith("ConanFile.") and method in qual
