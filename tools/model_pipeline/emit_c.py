#!/usr/bin/env python3
"""Deterministic C/H emission for animated Saturn model assets (host-only).

Extends the static compiled-model format (same table layout and formatting
conventions as tools/import_model.py) with baked animation assets:

- static tables: vertices (16.16 bind pose), A/B/C/D indices, per-face
  texture selectors, deduplicated baked textures, shared palette -- or, for
  solid-color assets, no textures and a baked-lighting shade palette;
- animation tables: one int16 pose stream per clip (frame-major), optional
  per-frame face shade indices, clip descriptors (frames, rate, loop flag,
  scale/bias encoding);
- one top-level ``sat_animated_model_asset_t`` descriptor.

No source UVs, joints, weights, inverse bind matrices or GLB structures
reach the runtime asset.
"""

from __future__ import annotations

from pathlib import Path

import sys

sys.path.insert(0, str(Path(__file__).parent.parent))
from saturn_asset_common import (
    format_byte_array,
    format_ushort_array,
    format_word_array,
    sanitize_identifier,
)


def _short_lines(values: list[int], indent: str = "    ", columns: int = 8) -> str:
    if not values:
        return ""
    lines: list[str] = []
    for start in range(0, len(values), columns):
        lines.append(indent + ", ".join(str(int(v)) for v in values[start : start + columns]))
    return ",\n".join(lines)


def emit_animated_c_h(
    static_result,
    animations: list[dict],
    out_prefix: Path,
    symbol: str | None,
) -> tuple[Path, Path]:
    """Write ``<prefix>.h/.c`` for one static model plus baked clips.

    ``static_result`` is the import_model.ImportResult shape (vertices_fx,
    indices_abcd, face_texture_indices, textures, palette_rgb555,
    palette_base, stats, shade_palette_rgb555). ``animations`` holds
    pose_bake.quantize_frames dicts plus ``name``, ``sample_rate_num/den``,
    ``loop`` and, for solid-color assets, ``shades``.
    """
    sym = sanitize_identifier(symbol) if symbol else sanitize_identifier(out_prefix.name)
    guard = f"{sym.upper()}_H"
    header_path = out_prefix.with_suffix(".h")
    source_path = out_prefix.with_suffix(".c")
    header_name = header_path.name

    nv = len(static_result.vertices_fx)
    nf = len(static_result.indices_abcd)
    nt = len(static_result.textures)
    na = len(animations)
    has_palette = bool(static_result.palette_rgb555)
    luts = list(getattr(static_result, "luts_rgb555", None) or [])
    nl = len(luts) // 16
    shade = list(getattr(static_result, "shade_palette_rgb555", None) or [])
    base = list(getattr(static_result, "face_base_shades", None) or [])

    header_lines = [
        f"#ifndef {guard}",
        f"#define {guard}",
        "",
        "#include <stdint.h>",
        "",
        '#include "saturn/model3d.h"',
        '#include "saturn/anim3d.h"',
        "",
        "#ifdef __cplusplus",
        'extern "C" {',
        "#endif",
        "",
        f"extern const sat_vec3_t {sym}_vertices[{nv}];",
        f"extern const uint16_t {sym}_indices[{nf * 4}];",
        f"extern const uint16_t {sym}_face_textures[{nf}];",
    ]
    if nt:
        header_lines.append(f"extern const sat_model_texture_asset_t {sym}_textures[{nt}];")
    if has_palette:
        header_lines.append("extern const uint16_t " + f"{sym}_palette[256];")
    if nl:
        header_lines.append(f"extern const uint16_t {sym}_luts[{nl * 16}];")
    if shade:
        header_lines.append(f"extern const uint16_t {sym}_shade_palette[{len(shade)}];")
    if base:
        header_lines.append(f"extern const uint8_t {sym}_face_base_shades[{nf}];")
    header_lines.append(f"extern const sat_model_asset_t {sym}_asset;")
    for i, anim in enumerate(animations):
        n = anim["frame_count"] * anim["vertex_count"] * 3
        header_lines.append(
            f"extern const int16_t {sym}_anim{i}_positions[{n}];"
        )
        if anim.get("shades") is not None:
            header_lines.append(
                f"extern const uint8_t {sym}_anim{i}_shades[{len(anim['shades'])}];"
            )
        if anim.get("vertex_gouraud") is not None:
            header_lines.append(
                f"extern const uint8_t {sym}_anim{i}_gouraud[{len(anim['vertex_gouraud'])}];"
            )
    header_lines += [
        f"extern const sat_model_animation_asset_t {sym}_animations[{na}];",
        f"extern const sat_animated_model_asset_t {sym}_anim_asset;",
        f"#define {sym.upper()}_VERTEX_COUNT ({nv}u)",
        f"#define {sym.upper()}_FACE_COUNT ({nf}u)",
        f"#define {sym.upper()}_TEXTURE_COUNT ({nt}u)",
        f"#define {sym.upper()}_SHADE_COUNT ({len(shade)}u)",
        f"#define {sym.upper()}_ANIMATION_COUNT ({na}u)",
    ]
    for i, anim in enumerate(animations):
        header_lines.append(f"#define {sym.upper()}_ANIM{i}_FRAMES ({anim['frame_count']}u)")
    header_lines += [
        "",
        "#ifdef __cplusplus",
        "}",
        "#endif",
        "",
        f"#endif /* {guard} */",
        "",
    ]
    header_path.parent.mkdir(parents=True, exist_ok=True)
    header_path.write_text("\n".join(header_lines), encoding="utf-8")

    parts: list[str] = [f'#include "{header_name}"', ""]
    parts.append(f"const sat_vec3_t {sym}_vertices[{nv}] = {{")
    for (x, y, z) in static_result.vertices_fx:
        parts.append(f"    {{{x}, {y}, {z}}},")
    parts.append("};")
    parts.append("")
    flat_indices: list[int] = []
    for (a, b, c, d) in static_result.indices_abcd:
        flat_indices.extend([a, b, c, d])
    parts.append(f"const uint16_t {sym}_indices[{nf * 4}] = {{")
    body = format_ushort_array(flat_indices)
    if body:
        parts.append(body)
    parts.append("};")
    parts.append("")
    parts.append(f"const uint16_t {sym}_face_textures[{nf}] = {{")
    body = format_ushort_array(static_result.face_texture_indices)
    if body:
        parts.append(body)
    parts.append("};")
    parts.append("")
    for i, tex in enumerate(static_result.textures):
        pix = list(tex["pixels"])
        parts.append(f"static const uint8_t {sym}_tex{i}_pixels[{len(pix)}] = {{")
        body = format_byte_array(pix)
        if body:
            parts.append(body)
        parts.append("};")
        parts.append("")
    if nt:
        parts.append(f"const sat_model_texture_asset_t {sym}_textures[{nt}] = {{")
        for i, tex in enumerate(static_result.textures):
            parts.append(
                f"    {{{sym}_tex{i}_pixels, {tex['width']}u, {tex['height']}u, "
                f"{tex.get('palette_slot', 0)}u, {tex['flags']}u, {tex['pixel_count']}u}},"
            )
        parts.append("};")
        parts.append("")
    if has_palette:
        parts.append(f"const uint16_t {sym}_palette[256] = {{")
        body = format_word_array(static_result.palette_rgb555)
        if body:
            parts.append(body)
        parts.append("};")
        parts.append("")
    if nl:
        parts.append(f"const uint16_t {sym}_luts[{nl * 16}] = {{")
        parts.append(format_word_array(luts))
        parts.append("};")
        parts.append("")
    if shade:
        parts.append(f"const uint16_t {sym}_shade_palette[{len(shade)}] = {{")
        parts.append(format_word_array(shade))
        parts.append("};")
        parts.append("")
    if base:
        parts.append(f"const uint8_t {sym}_face_base_shades[{nf}] = {{")
        parts.append(format_byte_array(base))
        parts.append("};")
        parts.append("")
    parts.append(f"const sat_model_asset_t {sym}_asset = {{")
    parts.append(f"    {sym}_vertices,")
    parts.append(f"    {nv}u,")
    parts.append(f"    {sym}_indices,")
    parts.append(f"    {nf}u,")
    parts.append(f"    {sym}_face_textures,")
    parts.append(f"    {sym}_textures," if nt else "    0,")
    parts.append(f"    {nt}u,")
    parts.append(f"    {sym}_palette," if has_palette else "    0,")
    parts.append("    1u," if has_palette else "    0u,")
    parts.append(f"    {static_result.palette_base}u,")
    parts.append("    0u,")
    parts.append(f"    {sym}_shade_palette," if shade else "    0,")
    parts.append(f"    {len(shade)}u,")
    parts.append(f"    {sym}_face_base_shades," if base else "    0,")
    parts.append(f"    {sym}_luts," if nl else "    0,")
    parts.append(f"    {nl}u")
    parts.append("};")
    parts.append("")
    for i, anim in enumerate(animations):
        n = anim["frame_count"] * anim["vertex_count"] * 3
        parts.append(f"const int16_t {sym}_anim{i}_positions[{n}] = {{")
        body = _short_lines(anim["stream"])
        if body:
            parts.append(body)
        parts.append("};")
        parts.append("")
        if anim.get("shades") is not None:
            parts.append(f"const uint8_t {sym}_anim{i}_shades[{len(anim['shades'])}] = {{")
            body = format_byte_array(list(anim["shades"]))
            if body:
                parts.append(body)
            parts.append("};")
            parts.append("")
        if anim.get("vertex_gouraud") is not None:
            parts.append(
                f"const uint8_t {sym}_anim{i}_gouraud[{len(anim['vertex_gouraud'])}] = {{")
            body = format_byte_array(list(anim["vertex_gouraud"]))
            if body:
                parts.append(body)
            parts.append("};")
            parts.append("")
    parts.append(f"const sat_model_animation_asset_t {sym}_animations[{na}] = {{")
    for i, anim in enumerate(animations):
        enc_b, enc_s = anim["encoding"]["bias"], anim["encoding"]["scale"]
        loop = "0x0001u" if anim["loop"] else "0x0000u"
        shades_ref = f"{sym}_anim{i}_shades" if anim.get("shades") is not None else "0"
        gouraud_ref = (f"{sym}_anim{i}_gouraud"
                       if anim.get("vertex_gouraud") is not None else "0")
        parts.append(f"    {{{sym}_anim{i}_positions,")
        parts.append(f"     {anim['frame_count']}u, {anim['vertex_count']}u,")
        parts.append(f"     {anim['sample_rate_num']}u, {anim['sample_rate_den']}u,")
        parts.append(f"     {loop}, 0u,")
        parts.append(
            f"     {{{enc_b[0]}, {enc_b[1]}, {enc_b[2]}, "
            f"{enc_s[0]}, {enc_s[1]}, {enc_s[2]}}},"
        )
        parts.append(f"     {shades_ref}, {gouraud_ref}}},")
    parts.append("};")
    parts.append("")
    parts.append(f"const sat_animated_model_asset_t {sym}_anim_asset = {{")
    parts.append(f"    &{sym}_asset,")
    parts.append(f"    {sym}_animations,")
    parts.append(f"    {na}u,")
    parts.append("    0u")
    parts.append("};")
    parts.append("")
    source_path.write_text("\n".join(parts), encoding="utf-8")
    return header_path, source_path
