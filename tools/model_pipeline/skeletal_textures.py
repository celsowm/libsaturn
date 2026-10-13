"""Bake immutable textured render data alongside a CPU-skinned mesh."""

from __future__ import annotations

import math

from .animated_import import _glb_winding
from .errors import ImportError
from .saturn_profile import SaturnProfile, check_resources
from .static_import import ImportResult
from .texture_bake import (
    bake_face_rgba, build_shared_palette, conform_size, estimate_face_size,
    float_to_fx16, map_faces_to_indices,
)
from saturn_asset_common import (
    format_byte_array, format_ushort_array, format_word_array, rgb888_to_rgb555,
)


def build_render_asset(model, export, *, palette_index=1, texture_scale=1.0,
                       max_texture_width=504, max_texture_height=255,
                       sampling="nearest", reverse_winding=False,
                       texel_extent=None, source_surface=None, exposure=1.0,
                       studio=False, light_dir=(-0.5, 0.6, 0.8), ambient=0.65, diffuse=0.35):
    """Keep geometry, UVs and face textures in the skin's rigid-first order."""
    if not 0 <= palette_index <= 7 or texture_scale <= 0:
        raise ImportError("skeletal textures need palette-index 0..7 and positive texture-scale")
    if len(model.uvs) != len(model.vertices):
        raise ImportError("skeletal textures need one UV per source vertex")
    if len(model.tri_materials) != len(model.triangles):
        raise ImportError("skeletal textures need one material per triangle")
    if texel_extent is not None and texel_extent <= 0:
        raise ImportError("--texel-extent must be positive")
    order = export.skin.source_vertex
    positions = [model.vertices[i] for i in order]
    # Geometry sharing must not replace a seam's UVs with the representative
    # vertex's UV. Bake with original source corners, then remap geometry only.
    source_quads, quad_uvs = _glb_winding(model.triangles, model.uvs, reverse_winding)
    remap = export.skin.source_to_vertex or [order.index(i) for i in range(len(model.vertices))]
    quads = [tuple(remap[i] for i in q) for q in source_quads]
    surface = None
    if source_surface is not None:
        from .surface_bake import SurfaceBaker
        surface = SurfaceBaker(source_surface, exposure=exposure, studio=studio,
                               light_dir=light_dir, ambient=ambient, diffuse=diffuse,
                               reverse_winding=reverse_winding)
    density = None
    if texel_extent is not None:
        span = max(max(p[i] for p in model.vertices) - min(p[i] for p in model.vertices)
                   for i in range(3))
        density = texel_extent / max(span, 1e-9)
    baked, sizes = [], []
    for face, (quad, face_uv) in enumerate(zip(quads, quad_uvs)):
        mi = model.tri_materials[face]
        if not 0 <= mi < len(model.materials):
            raise ImportError(f"face {face}: invalid material {mi}")
        mat = model.materials[mi]
        ti = mat.get("texture", -1)
        if not 0 <= ti < len(model.textures):
            raise ImportError(f"face {face}: material has no base-color texture")
        tex = model.textures[ti]
        w, h = estimate_face_size(face_uv, tex.width, tex.height, texture_scale)
        if density is not None:
            a, b, c, d = (model.vertices[i] for i in source_quads[face])
            w = min(w, max(1, math.ceil(max(math.dist(a, b), math.dist(d, c)) * density)))
            h = min(h, max(1, math.ceil(max(math.dist(a, d), math.dist(b, c)) * density)))
        w, h = conform_size(w, h, max_texture_width, max_texture_height,
                            mat.get("name"), face)
        sizes.append((w, h))
        if surface is not None:
            baked.append(surface.bake([model.vertices[i] for i in source_quads[face]], mi, w, h, sampling))
        else:
            baked.append(bake_face_rgba(face_uv, tex.width, tex.height,
                                       tex.pixels_rgba, w, h, sampling))
    palette, transparent, _ = build_shared_palette(baked)
    indexed = map_faces_to_indices(baked, sizes, palette)
    rgb555 = [0 if transparent and i == 0 else rgb888_to_rgb555(*rgb)
              for i, rgb in enumerate(palette)]
    rgb555 += [0] * (256 - len(rgb555))
    textures, selectors, dedup = [], [], {}
    flags = 0 if transparent else 1
    for (w, h), pixels in zip(sizes, indexed):
        pixels = bytes(pixels)
        key = (w, h, pixels)
        if key not in dedup:
            dedup[key] = len(textures)
            textures.append(dict(width=w, height=h, pixels=pixels,
                                 pixel_count=w * h, flags=flags))
        selectors.append(dedup[key])
    payloads = [len(t["pixels"]) for t in textures]
    budget = check_resources(SaturnProfile(), faces=len(quads),
                             texture_payload_bytes=sum(payloads), texture_sizes=payloads)
    if not budget["passed"]:
        raise ImportError("skeletal texture budget: " + "; ".join(budget["failing_gates"]))
    budget["source_bounds_fx"] = [
        [float_to_fx16(extreme(p[axis] for p in model.vertices) * export.scale)
         for axis in range(3)] for extreme in (min, max)]
    return ImportResult(
        vertices_fx=[tuple(float_to_fx16(x * export.scale) for x in p) for p in positions],
        indices_abcd=quads, face_texture_indices=selectors, textures=textures,
        palette_rgb555=rgb555, palette_base=palette_index, stats=budget,
    )


def emit_render_tables(render, base):
    """Reuse the skeletal index table in a standard sat_model_asset_t."""
    nt, nv, nf = len(render.textures), len(render.vertices_fx), len(render.indices_abcd)
    header = [f"extern const sat_model_asset_t {base}_asset;",
              f"extern const sat_vec3_t {base}_bind_min, {base}_bind_max;",
              f"#define {base.upper()}_TEXTURE_COUNT {nt}"]
    source = [f"static const sat_vec3_t {base}_bind_vertices[{nv}] = {{"]
    source += ["    {%d, %d, %d}," % p for p in render.vertices_fx]
    source += ["};", f"static const uint16_t {base}_face_textures[{nf}] = {{",
               format_ushort_array(render.face_texture_indices), "};"]
    for label, bound in zip(("min", "max"), render.stats["source_bounds_fx"]):
        source.append(f"const sat_vec3_t {base}_bind_{label} = {{" +
                      ", ".join(map(str, bound)) + "};")
    for i, tex in enumerate(render.textures):
        source += [f"static const uint8_t {base}_tex{i}[{len(tex['pixels'])}] = {{",
                   format_byte_array(list(tex["pixels"])), "};"]
    source.append(f"static const sat_model_texture_asset_t {base}_textures[{nt}] = {{")
    for i, tex in enumerate(render.textures):
        source.append(f"    {{{base}_tex{i}, {tex['width']}, {tex['height']}, "
                      f"0, {tex['flags']}, {tex['pixel_count']}}},")
    source += ["};", f"static const uint16_t {base}_palette[256] = {{",
               format_word_array(render.palette_rgb555), "};",
               f"const sat_model_asset_t {base}_asset = {{",
               f"    {base}_bind_vertices, {nv}, {base}_indices, {nf},",
               f"    {base}_face_textures, {base}_textures, {nt},",
               f"    {base}_palette, 1, {render.palette_base}, 0,",
               "    0, 0, 0, 0, 0", "};", ""]
    return header, source
