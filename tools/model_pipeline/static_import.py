"""Static OBJ-to-Saturn model assembly and C/H emission."""
from __future__ import annotations
from dataclasses import dataclass
import math
from pathlib import Path
from . import intersections as intersections_mod
from .constants import VDP1_COMMAND_AREA_BYTES, VDP1_MAX_TEXTURE_HEIGHT, VDP1_MAX_TEXTURE_WIDTH, VDP1_VRAM_BYTES
from .errors import ImportError
from .obj_import import parse_mtl, parse_obj, resolve_material_texture
from .texture_bake import BakedFaceInput, apply_scale, bake_face_rgba, build_shared_palette, canonicalize_faces, conform_size, estimate_face_size, float_to_fx16, load_rgba_image, map_faces_to_indices
from saturn_asset_common import asset_symbol_prefix, format_byte_array, format_ushort_array, format_word_array, rgb888_to_rgb555, sanitize_identifier

@dataclass
class ImportResult:
    vertices_fx: list[tuple[int, int, int]]
    indices_abcd: list[tuple[int, int, int, int]]
    face_texture_indices: list[int]
    textures: list[dict]  # {width,height,pixels(bytes),flags,pixel_count}
    palette_rgb555: list[int]
    palette_base: int
    stats: dict
    # Solid-color assets: baked-lighting palette, [0] reserved (see
    # model_pipeline/face_colors.py). None for textured assets.
    shade_palette_rgb555: list[int] | None = None
    # Solid-color assets: each face's Gouraud base shade (palette index).
    face_base_shades: list[int] | None = None
    # LUT4 assets: 16 RGB555 entries per table, flattened.
    luts_rgb555: list[int] | None = None


def split_intersecting_faces(
    verts: list[tuple[float, float, float]],
    faces: list[BakedFaceInput],
    face_images: list,
    max_faces: int,
):
    """Split faces that pass through each other (see model_pipeline.intersections).

    Returns (verts, faces, face_images, report). Faces that need no split keep
    their exact BakedFaceInput, so a model without intersections is emitted
    bit-identically; pieces reuse an existing vertex when a corner lands on
    one exactly and append a new vertex otherwise.
    """
    work = []
    for i, face in enumerate(faces):
        count = 3 if face.is_triangle else 4
        work.append(intersections_mod.Face(
            [(verts[face.vert_ids[k]], face.uvs[k]) for k in range(count)], i))
    originals = {id(f): i for i, f in enumerate(work)}
    try:
        pieces, report = intersections_mod.split_intersections(work, max_faces)
    except intersections_mod.SplitBudgetError as exc:
        raise ImportError(f"--split-intersections: {exc}; raise --split-face-budget") from exc
    if report["splits"] == 0:
        return verts, faces, face_images, report

    verts = list(verts)
    vertex_ids = {v: i for i, v in enumerate(verts)}
    out_faces: list[BakedFaceInput] = []
    out_images = []
    for piece in pieces:
        source = faces[piece.source]
        if id(piece) in originals:
            out_faces.append(source)
            out_images.append(face_images[piece.source])
            continue
        for corners in intersections_mod.to_corner_lists(piece):
            ids = []
            for pos, _ in corners:
                if pos not in vertex_ids:
                    vertex_ids[pos] = len(verts)
                    verts.append(pos)
                ids.append(vertex_ids[pos])
            uvs = [uv for _, uv in corners]
            if len(corners) == 3:
                ids.append(ids[2])
                uvs.append(uvs[2])
            out_faces.append(BakedFaceInput(
                tuple(ids), tuple(uvs), source.mtl, source.lineno, len(corners) == 3))
            out_images.append(face_images[piece.source])
    report["emitted_faces"] = len(out_faces)
    return verts, out_faces, out_images, report


def import_model(
    obj_path: Path,
    scale: float = 1.0,
    flip_x: bool = False,
    flip_y: bool = False,
    flip_z: bool = False,
    reverse_winding: bool = False,
    palette_index: int = 0,
    max_texture_width: int = VDP1_MAX_TEXTURE_WIDTH,
    max_texture_height: int = VDP1_MAX_TEXTURE_HEIGHT,
    texture_scale: float = 1.0,
    sampling: str = "nearest",
    split_intersections: bool = False,
    split_face_budget: int | None = None,
) -> ImportResult:
    if palette_index < 0 or palette_index > 7:
        raise ImportError(f"--palette-index must be in 0..7 (got {palette_index})")
    if texture_scale <= 0.0:
        raise ImportError(f"--texture-scale must be positive (got {texture_scale})")

    model = parse_obj(obj_path)
    if not model.vertices:
        raise ImportError(f"{obj_path}: no vertices found")
    if not model.faces:
        raise ImportError(f"{obj_path}: no faces found")
    if not model.mtllibs:
        raise ImportError(f"{obj_path}: no 'mtllib' statement; cannot resolve textures")
    if not model.uvs:
        raise ImportError(f"{obj_path}: no 'vt' UV coordinates; textured import needs UVs")

    obj_dir = obj_path.parent
    materials: dict[str, dict] = {}
    for lib in model.mtllibs:
        mtl_path = obj_dir / lib
        if not mtl_path.exists():
            raise ImportError(f"{obj_path}: MTL '{lib}' not found (looked at {mtl_path})")
        materials.update(parse_mtl(mtl_path))

    # Resolve each face's texture image (cached per material).
    image_cache: dict[str, tuple[int, int, list[tuple[int, int, int, int]]]] = {}
    face_images: list[tuple[int, int, list[tuple[int, int, int, int]]]] = []
    for face in model.faces:
        mtl = face["mtl"]
        tex_path = resolve_material_texture(mtl, materials, obj_dir, obj_path)
        key = str(tex_path.resolve())
        if key not in image_cache:
            image_cache[key] = load_rgba_image(tex_path)
        face_images.append(image_cache[key])

    verts, baked_inputs = canonicalize_faces(
        model, reverse_winding, flip_x, flip_y, flip_z
    )
    verts = apply_scale(verts, scale)
    split_report = None
    if split_intersections:
        budget = split_face_budget if split_face_budget is not None else 4 * len(baked_inputs)
        verts, baked_inputs, face_images, split_report = split_intersecting_faces(
            verts, baked_inputs, face_images, budget)

    # Bake every face to RGBA at its estimated resolution.
    baked_rgba: list[list[tuple[int, int, int, int]]] = []
    face_sizes: list[tuple[int, int]] = []
    for bface, (img_w, img_h, img_pixels) in zip(baked_inputs, face_images):
        est_w, est_h = estimate_face_size(bface.uvs, img_w, img_h, texture_scale)
        out_w, out_h = conform_size(est_w, est_h, max_texture_width, max_texture_height, bface.mtl, bface.lineno)
        rgba = bake_face_rgba(bface.uvs, img_w, img_h, img_pixels, out_w, out_h, sampling)
        baked_rgba.append(rgba)
        face_sizes.append((out_w, out_h))

    palette_rgb888, has_transparency, _ = build_shared_palette(baked_rgba)
    indexed_faces = map_faces_to_indices(baked_rgba, face_sizes, palette_rgb888)

    # Convert palette to RGB555. Index 0 is the transparent reservation when
    # transparency exists (emitted as 0x0000, never drawn); opaque entries
    # carry the RGB code bit so ordinary black stays distinct from it.
    palette_rgb555: list[int] = []
    for i, (r, g, b) in enumerate(palette_rgb888):
        if has_transparency and i == 0:
            palette_rgb555.append(0x0000)
        else:
            palette_rgb555.append(rgb888_to_rgb555(r, g, b))
    while len(palette_rgb555) < 256:
        palette_rgb555.append(0x0000)
    palette_rgb555 = palette_rgb555[:256]

    opaque_flag = 0x0001 if not has_transparency else 0x0000

    # Deduplicate after baking + palette mapping. The key includes everything
    # that changes interpretation: dims, indexed bytes, palette identity
    # (single shared palette here), and sprite flags.
    unique: list[dict] = []
    key_to_index: dict[tuple, int] = {}
    face_texture_indices: list[int] = []
    for (w, h), pixels in zip(face_sizes, indexed_faces):
        key = (w, h, bytes(pixels), 0, opaque_flag)
        if key in key_to_index:
            face_texture_indices.append(key_to_index[key])
        else:
            idx = len(unique)
            key_to_index[key] = idx
            unique.append(
                {"width": w, "height": h, "pixels": bytes(pixels), "flags": opaque_flag,
                 "pixel_count": w * h}
            )
            face_texture_indices.append(idx)

    vertices_fx = [
        (float_to_fx16(x), float_to_fx16(y), float_to_fx16(z)) for (x, y, z) in verts
    ]
    indices_abcd = [b.vert_ids for b in baked_inputs]

    indexed_bytes = sum(t["pixel_count"] for t in unique)
    vram_est = sum(((t["pixel_count"] + 7) & ~7) for t in unique)
    largest = max((t["width"] * t["height"], t["width"], t["height"]) for t in unique) if unique else (0, 0, 0)

    stats = {
        "source_vertices": len(model.vertices),
        "source_uvs": len(model.uvs),
        "source_faces": len(model.faces),
        "source_triangles": model.source_tris,
        "source_quads": model.source_quads,
        "materials": len(materials),
        "baked_faces_before_dedup": len(baked_rgba),
        "unique_textures_after_dedup": len(unique),
        "palette_count": 1,
        "indexed_texture_bytes": indexed_bytes,
        "palette_bytes": 512,
        "estimated_vram_usage": vram_est,
        "largest_baked_texture": (largest[1], largest[2]) if unique else (0, 0),
        "has_transparency": has_transparency,
    }
    if split_report is not None:
        stats["split_intersections"] = split_report

    return ImportResult(
        vertices_fx=vertices_fx,
        indices_abcd=indices_abcd,
        face_texture_indices=face_texture_indices,
        textures=unique,
        palette_rgb555=palette_rgb555,
        palette_base=palette_index,
        stats=stats,
    )


# ----------------------------------------------------------------------
# C/H emission (deterministic)
# ----------------------------------------------------------------------

def emit_c_h(
    result: ImportResult,
    out_prefix: Path,
    symbol: str | None,
) -> tuple[Path, Path]:
    sym = sanitize_identifier(symbol) if symbol else asset_symbol_prefix(out_prefix)
    guard = f"{sym.upper()}_H"
    header_path = out_prefix.with_suffix(".h")
    source_path = out_prefix.with_suffix(".c")
    header_name = header_path.name

    nv = len(result.vertices_fx)
    nf = len(result.indices_abcd)
    nt = len(result.textures)

    # Header: one top-level descriptor; application code never names faces.
    header_lines = [
        f"#ifndef {guard}",
        f"#define {guard}",
        "",
        "#include <stdint.h>",
        "",
        '#include "saturn/model3d.h"',
        "",
        "#ifdef __cplusplus",
        'extern "C" {',
        "#endif",
        "",
        f"extern const sat_vec3_t {sym}_vertices[{nv}];",
        f"extern const uint16_t {sym}_indices[{nf * 4}];",
        f"extern const uint16_t {sym}_face_textures[{nf}];",
        f"extern const sat_model_texture_asset_t {sym}_textures[{nt}];",
        "extern const uint16_t " + f"{sym}_palette[256];",
        f"extern const sat_model_asset_t {sym}_asset;",
        f"#define {sym.upper()}_VERTEX_COUNT ({nv}u)",
        f"#define {sym.upper()}_FACE_COUNT ({nf}u)",
        f"#define {sym.upper()}_TEXTURE_COUNT ({nt}u)",
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

    parts: list[str] = []
    parts.append(f'#include "{header_name}"')
    parts.append("")
    # Vertices as fixed-point constants (no runtime float parsing).
    parts.append(f"const sat_vec3_t {sym}_vertices[{nv}] = {{")
    for (x, y, z) in result.vertices_fx:
        parts.append(f"    {{{x}, {y}, {z}}},")
    parts.append("};")
    parts.append("")
    flat_indices: list[int] = []
    for (a, b, c, d) in result.indices_abcd:
        flat_indices.extend([a, b, c, d])
    parts.append(f"const uint16_t {sym}_indices[{nf * 4}] = {{")
    body = format_ushort_array(flat_indices)
    if body:
        parts.append(body)
    parts.append("};")
    parts.append("")
    parts.append(f"const uint16_t {sym}_face_textures[{nf}] = {{")
    body = format_ushort_array(result.face_texture_indices)
    if body:
        parts.append(body)
    parts.append("};")
    parts.append("")
    # One pixel array per unique texture, in stable first-appearance order.
    for i, tex in enumerate(result.textures):
        pix = list(tex["pixels"])
        parts.append(f"static const uint8_t {sym}_tex{i}_pixels[{len(pix)}] = {{")
        body = format_byte_array(pix)
        if body:
            parts.append(body)
        parts.append("};")
        parts.append("")
    parts.append(f"const sat_model_texture_asset_t {sym}_textures[{nt}] = {{")
    for i, tex in enumerate(result.textures):
        parts.append(
            f"    {{{sym}_tex{i}_pixels, {tex['width']}u, {tex['height']}u, "
            f"0u, {tex['flags']}u, {tex['pixel_count']}u}},"
        )
    parts.append("};")
    parts.append("")
    parts.append(f"const uint16_t {sym}_palette[256] = {{")
    body = format_word_array(result.palette_rgb555)
    if body:
        parts.append(body)
    parts.append("};")
    parts.append("")
    parts.append(f"const sat_model_asset_t {sym}_asset = {{")
    parts.append(f"    {sym}_vertices,")
    parts.append(f"    {nv}u,")
    parts.append(f"    {sym}_indices,")
    parts.append(f"    {nf}u,")
    parts.append(f"    {sym}_face_textures,")
    parts.append(f"    {sym}_textures,")
    parts.append(f"    {nt}u,")
    parts.append(f"    {sym}_palette,")
    parts.append("    1u,")
    parts.append(f"    {result.palette_base}u,")
    parts.append("    0u,")
    parts.append("    0,")
    parts.append("    0u,")
    parts.append("    0,")
    parts.append("    0,")
    parts.append("    0u")
    parts.append("};")
    parts.append("")
    source_path.write_text("\n".join(parts), encoding="utf-8")
    return header_path, source_path


def print_stats(stats: dict) -> None:
    print(f"source vertices: {stats['source_vertices']}")
    print(f"source UVs: {stats['source_uvs']}")
    print(f"source faces: {stats['source_faces']}")
    print(f"source triangles: {stats['source_triangles']}")
    print(f"source quads: {stats['source_quads']}")
    print(f"materials: {stats['materials']}")
    print(f"baked faces before dedup: {stats['baked_faces_before_dedup']}")
    print(f"unique textures after dedup: {stats['unique_textures_after_dedup']}")
    print(f"palette count: {stats['palette_count']}")
    print(f"indexed texture bytes: {stats['indexed_texture_bytes']}")
    print(f"palette bytes: {stats['palette_bytes']}")
    print(f"estimated VDP1 VRAM usage: {stats['estimated_vram_usage']}")
    lw, lh = stats["largest_baked_texture"]
    print(f"largest baked texture: {lw}x{lh}")
    split = stats.get("split_intersections")
    if split is not None:
        print(f"intersection splits: {split['splits']} "
              f"({split['input_faces']} -> {split.get('emitted_faces', split['output_faces'])} faces)")


