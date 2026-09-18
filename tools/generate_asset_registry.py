#!/usr/bin/env python3
"""Generate a bounded C registration function from an asset manifest.

The generated code contains no allocator or runtime filesystem dependency. It
registers embedded symbols directly and preserves physical paths as metadata
for a later CD/VFS transport. A physical entry is intentionally not treated as
resident data: typed loaders will return ``SAT_ERR_IO`` until a backend supplies
its payload.
"""

from __future__ import annotations

import argparse
import json
import re
from pathlib import Path
from typing import Any


KINDS = {
    "data": "SAT_ASSET_DATA",
    "texture": "SAT_ASSET_TEXTURE",
    "font": "SAT_ASSET_FONT",
    "sound": "SAT_ASSET_SOUND",
    "stream": "SAT_ASSET_STREAM",
}

FORMATS = {
    "index8": "SAT_PIXEL_INDEX8",
    "rgb555": "SAT_PIXEL_RGB555",
    "argb1555": "SAT_PIXEL_ARGB1555",
    "rgb565": "SAT_PIXEL_RGB565",
    "rgba8888": "SAT_PIXEL_RGBA8888",
    "s8": "SAT_AUDIO_PCM_S8",
    "pcm_s8": "SAT_AUDIO_PCM_S8",
    "s16": "SAT_AUDIO_PCM_S16",
    "pcm_s16": "SAT_AUDIO_PCM_S16",
}


def c_string(value: str) -> str:
    return json.dumps(value, ensure_ascii=True)


def c_identifier(value: str) -> bool:
    return re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*", value) is not None


def c_number(entry: dict[str, Any], field: str) -> str:
    value = entry.get(field, 0)
    if isinstance(value, bool) or not isinstance(value, int) or value < 0:
        raise ValueError(f"{field} must be a non-negative integer")
    return str(value)


def format_expr(value: Any) -> str:
    if value is None:
        return "0"
    if isinstance(value, bool):
        raise ValueError("format must be a string or integer")
    if isinstance(value, int):
        if value < 0:
            raise ValueError("format must be non-negative")
        return str(value)
    key = str(value).lower()
    if key not in FORMATS:
        raise ValueError(f"unsupported format: {value!r}")
    return FORMATS[key]


def load_entries(path: Path) -> list[dict[str, Any]]:
    source = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(source, dict) or not isinstance(source.get("assets"), list):
        raise ValueError("manifest must contain an 'assets' list")
    entries = source["assets"]
    normalized: list[dict[str, Any]] = []
    seen: set[str] = set()
    for entry in entries:
        if not isinstance(entry, dict):
            raise ValueError("each asset must be an object")
        logical = entry.get("logical_path")
        kind = str(entry.get("kind", "")).lower()
        if not isinstance(logical, str) or not logical or logical in seen:
            raise ValueError(f"invalid or duplicate logical_path: {logical!r}")
        if kind not in KINDS:
            raise ValueError(f"unsupported asset kind for {logical}: {kind!r}")
        physical = entry.get("physical_path")
        symbol = entry.get("embedded_symbol")
        if (physical is None) == (symbol is None):
            raise ValueError(
                f"{logical} must provide exactly one of physical_path or embedded_symbol"
            )
        if symbol is not None and not c_identifier(str(symbol)):
            raise ValueError(f"invalid embedded_symbol for {logical}: {symbol!r}")
        if symbol is not None and "size" not in entry:
            raise ValueError(f"embedded asset {logical} requires byte size")
        seen.add(logical)
        normalized.append(entry)
    return sorted(normalized, key=lambda item: item["logical_path"])


def render(entries: list[dict[str, Any]], function_name: str, header_name: str | None) -> str:
    if not c_identifier(function_name):
        raise ValueError(f"invalid function name: {function_name!r}")
    lines = [
        '#include "saturn/asset.h"',
        "",
    ]
    if header_name:
        lines.insert(0, f'#include "{header_name}"')
        lines.insert(1, "")
    for entry in entries:
        symbol = entry.get("embedded_symbol")
        if symbol is not None:
            lines.append(f"extern const unsigned char {symbol}[];")
    if any(entry.get("embedded_symbol") is not None for entry in entries):
        lines.append("")
    lines.append(f"sat_result_t {function_name}(void) {{")
    for index, entry in enumerate(entries):
        logical = c_string(str(entry["logical_path"]))
        source = entry.get("physical_path")
        source_expr = c_string(str(source)) if source is not None else "0"
        data_expr = (
            f"(const void*){entry['embedded_symbol']}"
            if entry.get("embedded_symbol") is not None
            else "0"
        )
        lines.extend(
            [
                f"    sat_asset_desc_t asset_{index} = {{}};",
                f"    asset_{index}.logical_path = {logical};",
                f"    asset_{index}.source_path = {source_expr};",
                f"    asset_{index}.data = {data_expr};",
                f"    asset_{index}.size = {c_number(entry, 'size')};",
                f"    asset_{index}.pitch = {c_number(entry, 'pitch')};",
                f"    asset_{index}.width = (uint16_t){c_number(entry, 'width')};",
                f"    asset_{index}.height = (uint16_t){c_number(entry, 'height')};",
                f"    asset_{index}.sample_rate = {c_number(entry, 'sample_rate')};",
                f"    asset_{index}.sample_count = {c_number(entry, 'sample_count')};",
                f"    asset_{index}.channels = (uint8_t){c_number(entry, 'channels')};",
                f"    asset_{index}.format = (uint8_t){format_expr(entry.get('format'))};",
                f"    asset_{index}.flags = (uint16_t){c_number(entry, 'flags')};",
                f"    asset_{index}.kind = {KINDS[str(entry['kind']).lower()]};",
                f"    sat_asset_t handle_{index} = {{}};",
                f"    SAT_TRY(sat_asset_register(&asset_{index}, &handle_{index}));",
            ]
        )
    lines.extend(["    return SAT_OK;", "}", ""])
    return "\n".join(lines)


def render_header(function_name: str, guard: str) -> str:
    return (
        f"#ifndef {guard}\n#define {guard}\n\n"
        '#include "saturn/core.h"\n\n'
        f"sat_result_t {function_name}(void);\n\n"
        f"#endif /* {guard} */\n"
    )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--function", default="sat_asset_register_generated")
    parser.add_argument("--header-output", type=Path)
    parser.add_argument("--header-name", help="include name used by generated C")
    args = parser.parse_args()
    try:
        entries = load_entries(args.input)
        header_name = args.header_name
        if args.header_output is not None and header_name is None:
            header_name = args.header_output.name
        source = render(entries, args.function, header_name)
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(source, encoding="utf-8", newline="\n")
        if args.header_output is not None:
            guard = re.sub(r"[^A-Za-z0-9]", "_", args.header_output.name).upper() + "_"
            args.header_output.parent.mkdir(parents=True, exist_ok=True)
            args.header_output.write_text(
                render_header(args.function, guard), encoding="utf-8", newline="\n"
            )
    except (OSError, json.JSONDecodeError, TypeError, ValueError) as error:
        parser.error(str(error))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
