#!/usr/bin/env python3
"""Shared helpers for Saturn asset converters.

Small module factored out of tools/convert_indexed8.py so the 2D converter
and the 3D model importer share identifier sanitizing, RGB555 conversion
and C array formatting without either script growing into the other.
"""

from __future__ import annotations

import re
from pathlib import Path
from typing import Sequence


def rgb888_to_rgb555(r: int, g: int, b: int) -> int:
    r5 = (r >> 3) & 0x1F
    g5 = (g >> 3) & 0x1F
    b5 = (b >> 3) & 0x1F
    return 0x8000 | (b5 << 10) | (g5 << 5) | r5


def rgb888_to_rgb555_opaque(r: int, g: int, b: int) -> int:
    return rgb888_to_rgb555(r, g, b)


def sanitize_identifier(value: str) -> str:
    name = re.sub(r"[^0-9A-Za-z]+", "_", value)
    name = re.sub(r"_+", "_", name).strip("_")
    if not name:
        return "asset"
    if name[0].isdigit():
        name = f"_{name}"
    return name


def asset_symbol_prefix(out_prefix: Path) -> str:
    return sanitize_identifier(out_prefix.name)


def asset_header_guard(out_prefix: Path) -> str:
    return f"{asset_symbol_prefix(out_prefix).upper()}_H"


def format_byte_array(
    values: Sequence[int], indent: str = "    ", columns: int = 12
) -> str:
    if not values:
        return ""
    lines: list[str] = []
    for start in range(0, len(values), columns):
        chunk = values[start : start + columns]
        lines.append(indent + ", ".join(f"0x{value:02X}" for value in chunk))
    return ",\n".join(lines)


def format_word_array(
    values: Sequence[int], indent: str = "    ", columns: int = 8
) -> str:
    if not values:
        return ""
    lines: list[str] = []
    for start in range(0, len(values), columns):
        chunk = values[start : start + columns]
        lines.append(indent + ", ".join(f"0x{value:04X}" for value in chunk))
    return ",\n".join(lines)


def format_int_array(
    values: Sequence[int], indent: str = "    ", columns: int = 4
) -> str:
    if not values:
        return ""
    lines: list[str] = []
    for start in range(0, len(values), columns):
        chunk = values[start : start + columns]
        lines.append(indent + ", ".join(f"{int(value)}" for value in chunk))
    return ",\n".join(lines)


def format_ushort_array(
    values: Sequence[int], indent: str = "    ", columns: int = 8
) -> str:
    if not values:
        return ""
    lines: list[str] = []
    for start in range(0, len(values), columns):
        chunk = values[start : start + columns]
        lines.append(indent + ", ".join(f"{int(value)}u" for value in chunk))
    return ",\n".join(lines)
