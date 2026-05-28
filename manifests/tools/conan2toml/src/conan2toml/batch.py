"""Batch-convert a Conan Center checkout.

Walks `<cci>/recipes/<name>/<version>/conanfile.py`, converts each to a
cppup `package.toml`, writes it under `<out>/recipes/<name>/<ver>/`, and
registers the version in `<out>/index.yaml`. A `.conan2toml-progress`
file under `<out>/` records which (name, version) pairs already
converted so re-running picks up where it stopped.
"""

from __future__ import annotations

import hashlib
import json
import logging
import traceback
from dataclasses import dataclass, field
from pathlib import Path

from .conandata import load_conandata, patches_for_version, source_for_version
from .emitter import emit_toml
from .index import upsert_version
from .loader import load_conanfile
from .mapper import RecipeSnapshot, map_recipe
from .patches import (
    PatchVerifyError,
    enrich_patch_hashes,
    strip_internal_fields,
    verify_patches,
)

log = logging.getLogger(__name__)


@dataclass
class BatchReport:
    converted: list[tuple[str, str]] = field(default_factory=list)
    todos: list[tuple[str, str, list[str]]] = field(default_factory=list)
    failed: list[tuple[str, str, str]] = field(default_factory=list)

    @property
    def total(self) -> int:
        return len(self.converted) + len(self.failed)

    def as_dict(self) -> dict:
        return {
            "converted": [{"name": n, "version": v} for n, v in self.converted],
            "with_todos": [
                {"name": n, "version": v, "todos": t} for n, v, t in self.todos
            ],
            "failed": [{"name": n, "version": v, "error": e} for n, v, e in self.failed],
        }


def convert_batch(
    cci_recipes_dir: Path,
    out_dir: Path,
    *,
    cci_commit: str | None = None,
    resume: bool = True,
    name_filter: str | None = None,
    fetch_patches: bool = True,
    verify: bool = False,
) -> BatchReport:
    cci_recipes_dir = Path(cci_recipes_dir)
    out_dir = Path(out_dir)
    if not cci_recipes_dir.is_dir():
        raise FileNotFoundError(f"recipes dir not found: {cci_recipes_dir}")

    progress_file = out_dir / ".conan2toml-progress"
    done = _load_progress(progress_file) if resume else set()
    report = BatchReport()

    for conanfile_path, name, version, recipe_rel in _iter_recipes(cci_recipes_dir, name_filter):
        key = f"{name}/{version}"
        if key in done:
            continue
        try:
            _convert_one(
                conanfile_path,
                name=name,
                version=version,
                out_dir=out_dir,
                recipe_rel=recipe_rel,
                cci_commit=cci_commit,
                fetch_patches=fetch_patches,
                verify=verify,
                report=report,
            )
            done.add(key)
            _save_progress(progress_file, done)
        except Exception as exc:
            log.warning("conversion failed for %s/%s: %s", name, version, exc)
            report.failed.append((name, version, f"{exc.__class__.__name__}: {exc}"))
            if log.isEnabledFor(logging.DEBUG):
                log.debug(traceback.format_exc())
    return report


def _convert_one(
    conanfile_path: Path,
    *,
    name: str,
    version: str,
    out_dir: Path,
    recipe_rel: str,
    cci_commit: str | None,
    fetch_patches: bool,
    verify: bool,
    report: BatchReport,
) -> None:
    cf = load_conanfile(conanfile_path, version=version)
    conandata = load_conandata(conanfile_path.parent / "conandata.yml")
    snap = RecipeSnapshot(
        conanfile=cf,
        source=source_for_version(conandata, version),
        patches=patches_for_version(conandata, version),
    )
    data = map_recipe(snap, recipe_repo_path=recipe_rel, cci_commit=cci_commit)
    todos = data.pop("_todos", [])

    if fetch_patches:
        fetch_report = enrich_patch_hashes(data)
        for url, err in fetch_report.failed:
            log.warning("patch fetch failed for %s/%s: %s (%s)", name, version, url, err)
    if verify:
        try:
            results = verify_patches(data)
        except PatchVerifyError as exc:
            raise RuntimeError(f"verify-patches: {exc}") from exc
        bad = [r for r in results if not r.applied]
        if bad:
            details = "; ".join(f"{r.url} ({r.error})" for r in bad)
            raise RuntimeError(f"patches did not apply: {details}")
    strip_internal_fields(data)

    rendered = emit_toml(data, todos=todos)

    manifest_rel = Path("recipes") / name / version / "package.toml"
    manifest_abs = out_dir / manifest_rel
    manifest_abs.parent.mkdir(parents=True, exist_ok=True)
    manifest_abs.write_text(rendered)

    pkg = data["package"]
    upsert_version(
        out_dir / "index.yaml",
        name=name,
        version=version,
        manifest_relpath=str(manifest_rel).replace("\\", "/"),
        manifest_sha256=hashlib.sha256(rendered.encode()).hexdigest(),
        source_url=data["source"]["url"],
        source_ref=version,
        kind=pkg["kind"],
        description=pkg.get("description"),
        license_name=pkg.get("license"),
        homepage=pkg.get("homepage"),
        repository=pkg.get("repository"),
    )

    report.converted.append((name, version))
    if todos:
        report.todos.append((name, version, todos))


def _iter_recipes(recipes_dir: Path, name_filter: str | None):
    for name_dir in sorted(recipes_dir.iterdir()):
        if not name_dir.is_dir():
            continue
        if name_filter and name_dir.name != name_filter:
            continue
        for variant in sorted(name_dir.iterdir()):
            if not variant.is_dir():
                continue
            conanfile = variant / "conanfile.py"
            if not conanfile.exists():
                continue
            conandata = variant / "conandata.yml"
            if not conandata.exists():
                continue
            versions = _versions_from_conandata(conandata)
            recipe_rel = f"recipes/{name_dir.name}/{variant.name}"
            for v in versions:
                yield conanfile, name_dir.name, v, recipe_rel


def _versions_from_conandata(path: Path) -> list[str]:
    try:
        data = load_conandata(path)
    except Exception:
        return []
    return list((data.get("sources") or {}).keys())


def _load_progress(path: Path) -> set[str]:
    if not path.exists():
        return set()
    try:
        return set(json.loads(path.read_text()).get("done", []))
    except (json.JSONDecodeError, OSError):
        return set()


def _save_progress(path: Path, done: set[str]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps({"done": sorted(done)}, indent=2))
