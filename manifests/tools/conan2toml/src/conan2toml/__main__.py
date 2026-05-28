"""conan2toml CLI.

Usage:

    conan2toml path/to/conanfile.py -o path/to/package.toml \
        [--version 1.2.3] [--cci-commit <sha>] [--recipe-path recipes/<pkg>/all]

    conan2toml --batch <conan-center-index/recipes> -o <out-dir> \
        [--cci-commit <sha>] [--no-resume] [--only <name>] [--emit-index]
"""

from __future__ import annotations

import argparse
import hashlib
import json
import logging
import sys
from pathlib import Path

from .batch import convert_batch
from .conandata import load_conandata, patches_for_version, source_for_version
from .emitter import emit_toml
from .index import upsert_version
from .loader import LoaderError, load_conanfile
from .mapper import RecipeSnapshot, map_recipe
from .patches import (
    PatchVerifyError,
    enrich_patch_hashes,
    strip_internal_fields,
    verify_patches,
)


def build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(
        prog="conan2toml",
        description="Convert Conan v2 recipes to cppup package.toml manifests.",
    )
    p.add_argument("input", nargs="?", help="Path to a conanfile.py")
    p.add_argument("-o", "--output", help="Output path (single mode) or directory (batch mode).")
    p.add_argument("--version", help="Recipe version (when class-level version is absent).")
    p.add_argument(
        "--cci-commit",
        help="conan-center-index commit hash used to build patch URLs.",
    )
    p.add_argument(
        "--recipe-path",
        help="Recipe path inside conan-center-index, e.g. recipes/fmt/all.",
    )
    p.add_argument(
        "--batch",
        metavar="DIR",
        help="Batch-convert a conan-center-index recipes/ tree.",
    )
    p.add_argument(
        "--no-resume",
        action="store_true",
        help="Ignore the .conan2toml-progress file when batching.",
    )
    p.add_argument(
        "--only",
        metavar="NAME",
        help="Restrict batch run to a single package name.",
    )
    p.add_argument(
        "--emit-index",
        action="store_true",
        help="(Single mode) also upsert manifests/index.yaml at OUTPUT's parent registry root.",
    )
    p.add_argument(
        "--no-fetch-patches",
        action="store_true",
        help="Skip downloading patch files to compute sha256 (emit empty hashes instead).",
    )
    p.add_argument(
        "--verify-patches",
        action="store_true",
        help="Download the source archive and dry-apply each patch with patch-ng.",
    )
    p.add_argument("-v", "--verbose", action="store_true", help="Verbose logging.")
    return p


def main(argv: list[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    logging.basicConfig(
        level=logging.DEBUG if args.verbose else logging.INFO,
        format="%(levelname)s %(name)s: %(message)s",
    )

    if args.batch:
        return _run_batch(args)
    return _run_single(args)


def _run_single(args: argparse.Namespace) -> int:
    if not args.input or not args.output:
        print("error: single-file mode requires INPUT and -o OUTPUT", file=sys.stderr)
        return 2

    in_path = Path(args.input)
    out_path = Path(args.output)
    try:
        cf = load_conanfile(in_path, version=args.version)
    except LoaderError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1

    version = args.version or getattr(cf, "version", None)
    if not version:
        print("error: no version provided and recipe has no class-level version", file=sys.stderr)
        return 2

    conandata = load_conandata(in_path.parent / "conandata.yml")
    snap = RecipeSnapshot(
        conanfile=cf,
        source=source_for_version(conandata, version),
        patches=patches_for_version(conandata, version),
    )
    data = map_recipe(
        snap,
        recipe_repo_path=args.recipe_path,
        cci_commit=args.cci_commit,
    )
    todos = data.pop("_todos", [])

    if not args.no_fetch_patches:
        report = enrich_patch_hashes(data)
        for url, err in report.failed:
            print(f"warning: patch fetch failed {url}: {err}", file=sys.stderr)
    if args.verify_patches:
        try:
            results = verify_patches(data)
        except PatchVerifyError as exc:
            print(f"error: {exc}", file=sys.stderr)
            return 1
        bad = [r for r in results if not r.applied]
        for r in results:
            status = "ok" if r.applied else f"FAIL: {r.error}"
            print(f"  patch {r.url} -> {status}")
        if bad:
            return 1
    strip_internal_fields(data)

    rendered = emit_toml(data, todos=todos)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text(rendered)
    print(f"wrote {out_path} ({len(rendered)} bytes)")
    if todos:
        print(f"  with {len(todos)} TODO(s) needing manual review")

    if args.emit_index:
        index_path = _index_path_for(out_path)
        pkg = data["package"]
        upsert_version(
            index_path,
            name=pkg["name"],
            version=pkg["version"],
            manifest_relpath=str(out_path.relative_to(index_path.parent)).replace("\\", "/"),
            manifest_sha256=hashlib.sha256(rendered.encode()).hexdigest(),
            source_url=data["source"]["url"],
            source_ref=pkg["version"],
            kind=pkg["kind"],
            description=pkg.get("description"),
            license_name=pkg.get("license"),
            homepage=pkg.get("homepage"),
            repository=pkg.get("repository"),
        )
        print(f"updated {index_path}")
    return 0


def _run_batch(args: argparse.Namespace) -> int:
    if not args.output:
        print("error: --batch requires -o OUTPUT_DIR", file=sys.stderr)
        return 2
    report = convert_batch(
        Path(args.batch),
        Path(args.output),
        cci_commit=args.cci_commit,
        resume=not args.no_resume,
        name_filter=args.only,
        fetch_patches=not args.no_fetch_patches,
        verify=args.verify_patches,
    )
    print(json.dumps(report.as_dict(), indent=2, sort_keys=True))
    return 0 if not report.failed else 1


def _index_path_for(manifest_out: Path) -> Path:
    # Default: place index.yaml three levels up from
    # <root>/recipes/<name>/<ver>/package.toml.
    cur = manifest_out.parent
    for _ in range(3):
        cur = cur.parent
    return cur / "index.yaml"


if __name__ == "__main__":
    raise SystemExit(main())
