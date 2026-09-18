#!/usr/bin/env python3
"""Generate a deterministic logical-asset manifest for the Saturn build.

The input is intentionally declarative and format-neutral.  Runtime payloads
may be embedded symbols or files staged into the CD image; the logical path
and metadata remain stable across that conversion.

Example input::

    {
      "assets": [
        {"logical_path": "assets/player.png", "kind": "texture",
         "physical_path": "generated/player.bin", "format": "index8",
         "width": 32, "height": 32}
      ]
    }

Use ``--root`` to resolve physical files and add their SHA-256 and byte size.
The output is sorted by logical path and contains no filesystem-dependent
ordering or timestamps.
"""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
from typing import Any


KINDS = {"data", "texture", "font", "sound", "stream"}
METADATA_FIELDS = (
    "format",
    "width",
    "height",
    "pitch",
    "sample_rate",
    "sample_count",
    "channels",
    "flags",
    "version",
)


def normalize_path(value: str, field: str) -> str:
    candidate = value.replace("\\", "/")
    path = PurePosixPath(candidate)
    if not candidate or path.is_absolute() or any(part == ".." for part in path.parts):
        raise ValueError(f"{field} must be a relative path without '..': {value!r}")
    parts = [part for part in path.parts if part not in ("", ".")]
    if not parts:
        raise ValueError(f"{field} must not be empty")
    return "/".join(parts)


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def load_entries(source: dict[str, Any]) -> list[dict[str, Any]]:
    raw_assets = source.get("assets")
    if not isinstance(raw_assets, list):
        raise ValueError("input must contain an 'assets' list")
    entries: list[dict[str, Any]] = []
    seen: set[str] = set()
    for raw in raw_assets:
        if not isinstance(raw, dict):
            raise ValueError("each asset must be an object")
        logical = normalize_path(str(raw.get("logical_path", "")), "logical_path")
        if logical in seen:
            raise ValueError(f"duplicate logical_path: {logical}")
        seen.add(logical)
        kind = str(raw.get("kind", "")).lower()
        if kind not in KINDS:
            raise ValueError(f"unsupported asset kind for {logical}: {kind!r}")
        physical = raw.get("physical_path")
        symbol = raw.get("embedded_symbol")
        if (physical is None) == (symbol is None):
            raise ValueError(
                f"{logical} must provide exactly one of physical_path or embedded_symbol"
            )
        entry: dict[str, Any] = {
            "logical_path": logical,
            "kind": kind,
        }
        if physical is not None:
            entry["physical_path"] = normalize_path(str(physical), "physical_path")
        else:
            if not str(symbol):
                raise ValueError(f"embedded_symbol must not be empty for {logical}")
            entry["embedded_symbol"] = str(symbol)
        for field in METADATA_FIELDS:
            if field in raw:
                entry[field] = raw[field]
        if "sha256" in raw:
            entry["sha256"] = str(raw["sha256"])
        if "size" in raw:
            entry["size"] = int(raw["size"])
        entries.append(entry)
    return sorted(entries, key=lambda item: item["logical_path"])


def enrich_files(entries: list[dict[str, Any]], root: Path | None) -> None:
    if root is None:
        return
    for entry in entries:
        physical = entry.get("physical_path")
        if physical is None:
            continue
        path = root / physical
        if not path.is_file():
            raise ValueError(f"physical asset does not exist: {path}")
        entry["size"] = path.stat().st_size
        entry["sha256"] = sha256_file(path)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--root", type=Path, help="root used to hash physical_path entries")
    args = parser.parse_args()

    try:
        source = json.loads(args.input.read_text(encoding="utf-8"))
        if not isinstance(source, dict):
            raise ValueError("input root must be an object")
        entries = load_entries(source)
        enrich_files(entries, args.root)
    except (OSError, json.JSONDecodeError, TypeError, ValueError) as error:
        parser.error(str(error))

    output = {"version": 1, "assets": entries}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(
        json.dumps(output, indent=2, ensure_ascii=True) + "\n", encoding="utf-8"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
