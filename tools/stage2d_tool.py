#!/usr/bin/env python3
"""Compile a stage2d spec (terrain profiles, metatile maps, entity regions, animation clips, Bezier
arc-length tables) into C arrays for the LibSaturn 2D runtime modules.

  stage2d_tool.py build SPEC --out-dir DIR [--max-bytes N]
  stage2d_tool.py check SPEC [--max-bytes N]

`check` runs every validation without writing. The spec format is documented in
docs/STAGE2D_TOOLS.md. Importers for specific games emit this spec; nothing here knows any game.
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from stage2d import Stage2dError, build_stage, emit_c, load_spec  # noqa: E402
from stage2d.emit_c import size_report  # noqa: E402


def _report(stage, max_bytes) -> int:
    sizes = size_report(stage)
    total = sum(sizes.values())
    for section, size in sizes.items():
        print(f"[stage2d] {stage.name}: {section:<10} {size:>8} bytes")
    print(f"[stage2d] {stage.name}: total      {total:>8} bytes")
    if max_bytes is not None and total > max_bytes:
        print(f"[stage2d] error: {total} bytes exceeds the budget of {max_bytes}", file=sys.stderr)
        return 1
    return 0


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest="command", required=True)
    for name in ("build", "check"):
        p = sub.add_parser(name)
        p.add_argument("spec", type=Path)
        p.add_argument("--max-bytes", type=int, default=None, help="fail when the read-only data exceeds this budget")
        if name == "build":
            p.add_argument("--out-dir", type=Path, required=True)
    args = parser.parse_args(argv)
    try:
        stage = build_stage(load_spec(args.spec))
        if args.command == "build":
            header, source = emit_c(stage)
            args.out_dir.mkdir(parents=True, exist_ok=True)
            (args.out_dir / f"{stage.name}.h").write_text(header, encoding="utf-8", newline="\n")
            (args.out_dir / f"{stage.name}.c").write_text(source, encoding="utf-8", newline="\n")
            print(f"[stage2d] wrote {args.out_dir / (stage.name + '.h')} and {args.out_dir / (stage.name + '.c')}")
        return _report(stage, args.max_bytes)
    except Stage2dError as exc:
        print(f"[stage2d] error: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
