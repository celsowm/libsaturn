#!/usr/bin/env python3
"""Print the runtime source lists from the canonical CMake manifest.

cmake/LibSaturnSources.cmake is the only hand-maintained inventory of the
reusable runtime. CMake includes it directly; the Makefile asks this script
for the same lists so the two builds cannot drift.

Usage:
    python tools/library_sources.py              # C and C++ library sources
    python tools/library_sources.py --startup    # startup assembly objects
    python tools/library_sources.py --all        # everything, one per line
"""
import argparse
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
MANIFEST = ROOT / "cmake" / "LibSaturnSources.cmake"

SET_RE = re.compile(r"set\(\s*(LIBSATURN_[A-Z0-9_]+_SOURCES)\b([^)]*)\)")


def parse_manifest(path=MANIFEST):
    """Return {variable: [repo-relative path, ...]} in manifest order."""
    text = re.sub(r"#[^\n]*", "", path.read_text(encoding="utf-8"))
    lists = {}
    for name, body in SET_RE.findall(text):
        lists[name] = body.split()
    return lists


def library_sources(lists):
    return [src for name, files in lists.items()
            if name != "LIBSATURN_STARTUP_SOURCES" for src in files]


def startup_sources(lists):
    return list(lists.get("LIBSATURN_STARTUP_SOURCES", []))


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    group = ap.add_mutually_exclusive_group()
    group.add_argument("--startup", action="store_true")
    group.add_argument("--all", action="store_true")
    ap.add_argument("--manifest", type=Path, default=MANIFEST)
    args = ap.parse_args()

    lists = parse_manifest(args.manifest)
    if not lists:
        print(f"error: no LIBSATURN_*_SOURCES in {args.manifest}",
              file=sys.stderr)
        return 1
    if args.startup:
        files = startup_sources(lists)
    elif args.all:
        files = library_sources(lists) + startup_sources(lists)
    else:
        files = library_sources(lists)
    # One line, space separated: consumed by $(shell ...) in the Makefile.
    print(" ".join(files))
    return 0


if __name__ == "__main__":
    sys.exit(main())
