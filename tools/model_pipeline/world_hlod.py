#!/usr/bin/env python3
"""Deterministic static-scene HLOD partitioning and packaging (host-only).

The converter accepts the shared :class:`SourceModel` IR. It clips triangles
to a regular 3D grid while interpolating UVs, simplifies every cell/LOD from
that cell's original clipped source, and writes a compact big-endian package.
It deliberately does not import assets, bake Saturn textures, or perform
runtime work; callers can pair its material IDs with the existing static
texture pipeline. No source assets are modified.
"""

from __future__ import annotations

import hashlib
import json
import math
import struct
from dataclasses import dataclass

from .gltf import GltfError
from .model import SourceModel

MAGIC = b"WLH1"
VERSION = 1
HEADER = struct.Struct(">4sHHI32s")
CELL = struct.Struct(">iii6iH")
LEVEL = struct.Struct(">HHIIII")
VERTEX = struct.Struct(">5i")  # cell-relative xyz and UV, signed 16.16
TRIANGLE = struct.Struct(">4H")  # three vertex indices and material ID
MAX_CELL_COUNT = 65535
MAX_PACKAGE_BYTES = 16 * 1024 * 1024
DEFAULT_TEXTURE_WIDTH = 504
DEFAULT_TEXTURE_HEIGHT = 255
FIXED_CAMERA_VIEWS = (
    ("front", (0.0, 0.0, -1.0)),
    ("oblique", (0.55, 0.18, -0.82)),
    ("elevated", (0.2, 0.88, -0.42)),
)


@dataclass(frozen=True)
class LodTarget:
    name: str
    triangles: int


@dataclass(frozen=True)
class ProtectedRegion:
    name: str
    minimum: tuple[float, float, float]
    maximum: tuple[float, float, float]


def _fx16(value: float, what: str) -> int:
    if not math.isfinite(value):
        raise GltfError(f"{what}: non-finite 16.16 value {value!r}")
    scaled = int(round(value * 65536.0))
    if scaled < -0x80000000 or scaled > 0x7FFFFFFF:
        raise GltfError(f"{what}: {value:g} does not fit signed 16.16")
    return scaled


def _source_checksum(source: SourceModel) -> str:
    textures = []
    for texture in source.textures:
        pixels = bytes(channel for pixel in texture.pixels_rgba for channel in pixel)
        textures.append({
            "width": texture.width,
            "height": texture.height,
            "mime": texture.mime,
            "pixels_sha256": hashlib.sha256(pixels).hexdigest(),
        })
    payload = {
        "vertices": source.vertices,
        "uvs": source.uvs,
        "triangles": source.triangles,
        "tri_materials": source.tri_materials,
        "materials": source.materials,
        "textures": textures,
    }
    canonical = json.dumps(payload, sort_keys=True, separators=(",", ":"),
                           ensure_ascii=True, allow_nan=False).encode("ascii")
    return hashlib.sha256(canonical).hexdigest()


def _validate_source(source: SourceModel, max_texture_width: int,
                     max_texture_height: int) -> dict:
    if not source.vertices or not source.triangles:
        raise GltfError("world_hlod: source has no vertices or triangles")
    if len(source.tri_materials) != len(source.triangles):
        raise GltfError("world_hlod: triangle material count does not match triangles")
    if source.uvs and len(source.uvs) != len(source.vertices):
        raise GltfError("world_hlod: UV count does not match vertices")
    if len(source.materials) > 65535:
        raise GltfError("world_hlod: material table exceeds the u16 package limit")
    for vi, position in enumerate(source.vertices):
        if len(position) != 3 or any(not math.isfinite(float(v)) for v in position):
            raise GltfError(f"world_hlod: vertex {vi} has invalid coordinates")
    for ui, uv in enumerate(source.uvs):
        if len(uv) != 2 or any(not math.isfinite(float(v)) for v in uv):
            raise GltfError(f"world_hlod: UV {ui} is not a finite pair")
    for ti, triangle in enumerate(source.triangles):
        if len(triangle) != 3 or len(set(triangle)) != 3:
            raise GltfError(f"world_hlod: triangle {ti} is degenerate or malformed")
        if any(index < 0 or index >= len(source.vertices) for index in triangle):
            raise GltfError(f"world_hlod: triangle {ti} has an out-of-range vertex")
        p0, p1, p2 = (source.vertices[index] for index in triangle)
        e1 = tuple(p1[a] - p0[a] for a in range(3))
        e2 = tuple(p2[a] - p0[a] for a in range(3))
        cross = (e1[1]*e2[2]-e1[2]*e2[1], e1[2]*e2[0]-e1[0]*e2[2],
                 e1[0]*e2[1]-e1[1]*e2[0])
        if sum(v*v for v in cross) <= 1e-20:
            raise GltfError(f"world_hlod: triangle {ti} has zero area")
        material = source.tri_materials[ti]
        if material < -1 or material >= len(source.materials) or material > 65535:
            raise GltfError(f"world_hlod: triangle {ti} has invalid material {material}")

    texture_bytes = {}
    largest = [0, 0]
    for ti, texture in enumerate(source.textures):
        if texture.width < 1 or texture.height < 1:
            raise GltfError(f"world_hlod: texture {ti} has an empty extent")
        if (texture.width % 8 or texture.width > max_texture_width
                or texture.height > max_texture_height):
            raise GltfError(
                f"world_hlod: texture {ti} extent {texture.width}x{texture.height} "
                f"exceeds VDP1 limits {max_texture_width}x{max_texture_height}"
            )
        if len(texture.pixels_rgba) != texture.width * texture.height:
            raise GltfError(f"world_hlod: texture {ti} pixel count does not match its extent")
        if any(len(pixel) != 4 or any(not 0 <= int(c) <= 255 for c in pixel)
               for pixel in texture.pixels_rgba):
            raise GltfError(f"world_hlod: texture {ti} contains an invalid RGBA pixel")
        texture_bytes[ti] = texture.width * texture.height * 4
        largest = [max(largest[0], texture.width), max(largest[1], texture.height)]

    material_texture_ids: set[int] = set()
    palette_entries = 0
    for mi, material in enumerate(source.materials):
        if not isinstance(material, dict):
            raise GltfError(f"world_hlod: material {mi} metadata must be a mapping")
        texture_index = material.get("texture_index")
        if texture_index is not None:
            if not isinstance(texture_index, int) or texture_index not in texture_bytes:
                raise GltfError(f"world_hlod: material {mi} references an invalid texture")
            material_texture_ids.add(texture_index)
        count = material.get("palette_entries", 0)
        if not isinstance(count, int) or count < 0 or count > 256:
            raise GltfError(f"world_hlod: material {mi} palette_entries must be in 0..256")
        palette_entries = max(palette_entries, count)
    return {
        "texture_bytes": sum(texture_bytes[i] for i in sorted(material_texture_ids)),
        "largest_texture_extent": largest,
        "palette_entries": palette_entries,
        "material_indices_valid": True,
        "texture_references_valid": True,
        "uvs_finite": True,
    }


def _clip_plane(poly: list[tuple[tuple[float, float, float], tuple[float, float]]],
                axis: int, bound: float, keep_greater: bool):
    if not poly:
        return []
    out = []

    def inside(corner):
        value = corner[0][axis] - bound
        return value >= 0.0 if keep_greater else value <= 0.0

    previous = poly[-1]
    prev_in = inside(previous)
    for current in poly:
        cur_in = inside(current)
        if cur_in != prev_in:
            delta = current[0][axis] - previous[0][axis]
            if abs(delta) > 1e-15:
                t = (bound - previous[0][axis]) / delta
                pos = [previous[0][k] + t * (current[0][k] - previous[0][k])
                       for k in range(3)]
                pos[axis] = bound
                uv = tuple(previous[1][k] + t * (current[1][k] - previous[1][k])
                           for k in range(2))
                out.append((tuple(pos), uv))
        if cur_in:
            out.append(current)
        previous, prev_in = current, cur_in
    return out


def _axis_cells(lo: float, hi: float, size: float) -> range:
    low = math.floor(lo / size)
    high = math.floor(hi / size)
    # A polygon that lies exactly on a grid plane belongs to the lower cell,
    # preventing duplicate geometry while keeping ordinary crossings shared.
    if abs(hi - lo) <= 1e-10 and abs(lo / size - round(lo / size)) <= 1e-10:
        low = high = int(round(lo / size)) - 1
    return range(low, high + 1)


def partition_source(source: SourceModel, cell_size: float) -> dict[tuple[int, int, int], SourceModel]:
    """Clip original source triangles into deterministic 3D cells."""
    if not math.isfinite(cell_size) or cell_size <= 0:
        raise GltfError("world_hlod: cell_size must be finite and positive")
    uv_source = source.uvs or [(0.0, 0.0)] * len(source.vertices)
    cells: dict[tuple[int, int, int], dict] = {}
    for ti, triangle in enumerate(source.triangles):
        corners = [(tuple(map(float, source.vertices[vi])), tuple(map(float, uv_source[vi])))
                   for vi in triangle]
        xs, ys, zs = ([corner[0][axis] for corner in corners] for axis in range(3))
        axes = (_axis_cells(min(xs), max(xs), cell_size),
                _axis_cells(min(ys), max(ys), cell_size),
                _axis_cells(min(zs), max(zs), cell_size))
        candidate_count = len(axes[0]) * len(axes[1]) * len(axes[2])
        if candidate_count > 4096:
            raise GltfError(f"world_hlod: triangle {ti} spans {candidate_count} cells")
        for ix in axes[0]:
            for iy in axes[1]:
                for iz in axes[2]:
                    key = (ix, iy, iz)
                    bounds = [(key[k] * cell_size, (key[k] + 1) * cell_size)
                              for k in range(3)]
                    poly = corners
                    for axis, (lo, hi) in enumerate(bounds):
                        poly = _clip_plane(poly, axis, lo, True)
                        poly = _clip_plane(poly, axis, hi, False)
                        if len(poly) < 3:
                            break
                    if len(poly) < 3:
                        continue
                    bucket = cells.setdefault(key, {"vertices": [], "uvs": [],
                                                     "triangles": [], "materials": [],
                                                     "source_ids": set()})
                    ids = {}

                    def vertex_id(corner):
                        p, uv = corner
                        token = tuple(round(v, 12) for v in p + uv)
                        if token not in ids:
                            ids[token] = len(bucket["vertices"])
                            bucket["vertices"].append(tuple(p))
                            bucket["uvs"].append(tuple(uv))
                        return ids[token]

                    emitted = False
                    for i in range(1, len(poly) - 1):
                        p1, p2 = poly[i][0], poly[i + 1][0]
                        e1 = tuple(p1[k] - poly[0][0][k] for k in range(3))
                        e2 = tuple(p2[k] - poly[0][0][k] for k in range(3))
                        cross_piece = (e1[1]*e2[2]-e1[2]*e2[1],
                                       e1[2]*e2[0]-e1[0]*e2[2],
                                       e1[0]*e2[1]-e1[1]*e2[0])
                        if sum(v*v for v in cross_piece) <= 1e-20:
                            continue
                        # A boundary touch may clip to a zero-area polygon.
                        # Allocate corners only for accepted triangles so its
                        # orphan points cannot pollute bounds/quality samples.
                        bucket["triangles"].append((vertex_id(poly[0]), vertex_id(poly[i]), vertex_id(poly[i + 1])))
                        bucket["materials"].append(source.tri_materials[ti])
                        emitted = True
                    if emitted:
                        bucket["source_ids"].add(ti)

    if not cells:
        raise GltfError("world_hlod: partition produced no non-degenerate cells")
    if len(cells) > MAX_CELL_COUNT:
        raise GltfError(f"world_hlod: {len(cells)} cells exceeds package limit {MAX_CELL_COUNT}")
    result = {}
    for key in sorted(cells):
        if any(index < -0x80000000 or index > 0x7FFFFFFF for index in key):
            raise GltfError(f"world_hlod: grid coordinate {key} does not fit signed 32-bit metadata")
        if any(not math.isfinite(index * cell_size) for index in key):
            raise GltfError(f"world_hlod: cell origin {key} is not finite")
        value = cells[key]
        if not value["triangles"]:
            continue
        result[key] = SourceModel(vertices=value["vertices"], uvs=value["uvs"],
                                  triangles=value["triangles"],
                                  tri_materials=value["materials"],
                                  materials=source.materials, textures=source.textures)
        # Host-only provenance used for per-cell source coverage reporting.
        result[key].source_triangle_ids = sorted(value["source_ids"])
    return result


def _regions_from(values) -> list[ProtectedRegion]:
    out = []
    for item in values or ():
        region = item if isinstance(item, ProtectedRegion) else ProtectedRegion(
            str(item["name"]), tuple(map(float, item["minimum"])),
            tuple(map(float, item["maximum"])))
        if (len(region.minimum) != 3 or len(region.maximum) != 3
                or any(not math.isfinite(v) for v in region.minimum + region.maximum)
                or any(region.minimum[i] > region.maximum[i] for i in range(3))):
            raise GltfError(f"world_hlod: invalid protected region {region.name!r}")
        out.append(region)
    return out


def _protected_importance(model: SourceModel, key, cell_size: float,
                          regions: list[ProtectedRegion]) -> list[float]:
    from . import silhouette

    scores = silhouette.compute_silhouette_importance(model, n_views=12)
    cell_origin = tuple(v * cell_size for v in key)
    for vi, point in enumerate(model.vertices):
        world = tuple(point[a] + cell_origin[a] for a in range(3))
        for region in regions:
            if all(region.minimum[a] <= world[a] <= region.maximum[a] for a in range(3)):
                scores[vi] = 1.0
                break
    return scores


def _texture_metrics(source: SourceModel, cell: SourceModel, validation: dict) -> dict:
    used = sorted({m for m in cell.tri_materials if m >= 0})
    texture_indices = sorted({source.materials[m].get("texture_index") for m in used
                              if source.materials[m].get("texture_index") is not None})
    return {
        "texture_bytes": sum(source.textures[i].width * source.textures[i].height * 4
                             for i in texture_indices),
        "largest_texture_extent": validation["largest_texture_extent"],
        "palette_validation": {
            "material_indices_valid": validation["material_indices_valid"],
            "texture_references_valid": validation["texture_references_valid"],
            "palette_entries_max": validation["palette_entries"],
        },
        "uv_validation": {"finite": validation["uvs_finite"],
                           "vertices": len(cell.uvs),
                           "max_abs": max((abs(v) for uv in cell.uvs for v in uv), default=0.0)},
    }


def _mesh_payload(mesh, cell_key, cell_size: float) -> bytes:
    origin = tuple(k * cell_size for k in cell_key)
    if len(mesh.positions) > 65535 or len(mesh.triangles) > 65535:
        raise GltfError("world_hlod: one cell LOD exceeds u16 vertex/face counts")
    out = bytearray()
    uvs = mesh.uvs if mesh.uvs else [(0.0, 0.0)] * len(mesh.positions)
    if len(uvs) != len(mesh.positions):
        raise GltfError("world_hlod: simplified UV/vertex count mismatch")
    for position, uv in zip(mesh.positions, uvs):
        values = [position[i] - origin[i] for i in range(3)] + list(uv)
        out.extend(VERTEX.pack(*(_fx16(v, "cell-local vertex/UV") for v in values)))
    if len(mesh.tri_materials) != len(mesh.triangles):
        raise GltfError("world_hlod: simplified material/triangle count mismatch")
    for triangle, material in zip(mesh.triangles, mesh.tri_materials):
        if any(index < 0 or index >= len(mesh.positions) for index in triangle):
            raise GltfError("world_hlod: simplifier produced an invalid triangle index")
        if any(index > 65535 for index in triangle) or material < -1 or material > 65535:
            raise GltfError("world_hlod: triangle index/material does not fit u16")
        out.extend(TRIANGLE.pack(*triangle, 0xFFFF if material == -1 else material))
    return bytes(out)


def _fixed_camera_view_error(source: SourceModel, simplified, size: int = 96) -> dict:
    """Source-to-variant silhouette metrics at named, reproducible views."""
    from . import silhouette as silhouette_mod

    views = [view for _name, view in FIXED_CAMERA_VIEWS]
    positions = source.vertices
    lo = tuple(min(p[a] for p in positions) for a in range(3))
    hi = tuple(max(p[a] for p in positions) for a in range(3))
    frame = (lo, hi)
    source_masks = silhouette_mod.render_masks(
        source.vertices, source.triangles, views, size=size, frame=frame)
    variant_masks = silhouette_mod.render_masks(
        simplified.positions, simplified.triangles, views, size=size, frame=frame)
    cameras = []
    for (name, direction), source_mask, variant_mask in zip(
            FIXED_CAMERA_VIEWS, source_masks, variant_masks):
        intersection = int((source_mask & variant_mask).sum())
        union = int((source_mask | variant_mask).sum())
        iou = intersection / union if union else 1.0
        chamfer = silhouette_mod.contour_chamfer(source_mask, variant_mask)
        if not math.isfinite(chamfer):
            raise GltfError(f"world_hlod: {name} camera lost the complete source silhouette")
        cameras.append({"name": name, "direction": list(direction), "iou": iou,
                        "chamfer_px": chamfer})
    return {
        "size": size,
        "cameras": cameras,
        "iou_min": min(c["iou"] for c in cameras),
        "iou_mean": sum(c["iou"] for c in cameras) / len(cameras),
        "chamfer_px_max": max(c["chamfer_px"] for c in cameras),
        "chamfer_px_mean": sum(c["chamfer_px"] for c in cameras) / len(cameras),
    }


def build_package(source: SourceModel, *, cell_size: float = 32.0,
                  lod_targets: tuple[LodTarget, ...] = (
                      LodTarget("lod0", 1_000_000),
                      LodTarget("lod1", 700_000),
                      LodTarget("lod2", 450_000)),
                  quality: str = "balanced", protected_regions=(),
                  max_texture_width: int = DEFAULT_TEXTURE_WIDTH,
                  max_texture_height: int = DEFAULT_TEXTURE_HEIGHT,
                  max_package_bytes: int = MAX_PACKAGE_BYTES,
                  silhouette_views: int = 12) -> tuple[bytes, dict]:
    """Return ``(WLOD bytes, metrics report)`` or reject invalid/low-quality data."""
    from . import lod as lod_mod
    from . import silhouette as silhouette_mod

    validation = _validate_source(source, max_texture_width, max_texture_height)
    if not lod_targets:
        raise GltfError("world_hlod: at least one LOD target is required")
    targets = tuple(lod_targets)
    if len(targets) > 65535:
        raise GltfError("world_hlod: LOD count exceeds u16 package limit")
    if any(not item.name or item.triangles < 1 for item in targets):
        raise GltfError("world_hlod: LOD names and positive triangle targets are required")
    if any(targets[i].triangles < targets[i + 1].triangles
           for i in range(len(targets) - 1)):
        raise GltfError("world_hlod: targets must be non-increasing by LOD")
    if max_package_bytes < HEADER.size or max_package_bytes > MAX_PACKAGE_BYTES:
        raise GltfError(f"world_hlod: max package size must be in {HEADER.size}..{MAX_PACKAGE_BYTES}")
    if quality not in ("conservative", "balanced", "aggressive"):
        raise GltfError(f"world_hlod: unknown simplification quality {quality!r}")
    regions = _regions_from(protected_regions)
    cells = partition_source(source, cell_size)
    if not 1 <= silhouette_views <= 64:
        raise GltfError("world_hlod: silhouette_views must be in 1..64")
    source_hash = _source_checksum(source)
    cell_reports = []
    cell_records = []
    payloads = []
    source_area = 0.0
    for triangle in source.triangles:
        p0, p1, p2 = (source.vertices[i] for i in triangle)
        e1 = tuple(p1[a]-p0[a] for a in range(3))
        e2 = tuple(p2[a]-p0[a] for a in range(3))
        cross = (e1[1]*e2[2]-e1[2]*e2[1], e1[2]*e2[0]-e1[0]*e2[2],
                 e1[0]*e2[1]-e1[1]*e2[0])
        source_area += 0.5 * math.sqrt(sum(v*v for v in cross))
    source_ids_covered = set()

    for key, cell_source in sorted(cells.items()):
        cell_source_hash = _source_checksum(cell_source)
        source_ids_covered.update(cell_source.source_triangle_ids)
        area_by_cell = 0.0
        for triangle in cell_source.triangles:
            p0, p1, p2 = (cell_source.vertices[i] for i in triangle)
            e1 = tuple(p1[a]-p0[a] for a in range(3))
            e2 = tuple(p2[a]-p0[a] for a in range(3))
            cross = (e1[1]*e2[2]-e1[2]*e2[1], e1[2]*e2[0]-e1[0]*e2[2],
                     e1[0]*e2[1]-e1[1]*e2[0])
            area_by_cell += 0.5 * math.sqrt(sum(v*v for v in cross))
        # Simplify every requested level from this unchanged, clipped source.
        sil_importance = _protected_importance(cell_source, key, cell_size, regions)
        specs = [lod_mod.LodSpec(t.name, min(t.triangles, len(cell_source.triangles)), quality)
                 for t in targets]
        variants = lod_mod.generate_lods(cell_source, None, specs,
                                         sil_importance=sil_importance,
                                         silhouette_views=silhouette_views)
        if any(variants[i]["delivered_triangles"] < variants[i + 1]["delivered_triangles"]
               for i in range(len(variants) - 1)):
            raise GltfError(f"world_hlod: non-monotonic delivered LOD costs in cell {key}")

        origin = tuple(v * cell_size for v in key)
        positions = cell_source.vertices
        bounds_world = tuple(min(p[a] for p in positions) for a in range(3)) + tuple(
            max(p[a] for p in positions) for a in range(3))
        bounds_local = [bounds_world[a] - origin[a] for a in range(3)] + [
            bounds_world[a + 3] - origin[a] for a in range(3)]
        bounds_fx = tuple(_fx16(v, "cell-local AABB") for v in bounds_local)
        records = []
        variants_report = []
        for level_index, variant in enumerate(variants):
            quality_report = variant["quality"]
            if not quality_report["passed"]:
                failures = "; ".join(quality_report["failing_gates"])
                raise GltfError(f"world_hlod: {key}/{targets[level_index].name} failed quality gates: {failures}")
            mesh = variant["simplified"]
            target = targets[level_index].triangles
            budget_failures = []
            if len(mesh.triangles) > target:
                budget_failures.append(
                    f"delivered {len(mesh.triangles)} faces exceeds target {target}"
                )
            if budget_failures:
                raise GltfError(
                    f"world_hlod: {key}/{targets[level_index].name} budget failure: "
                    + "; ".join(budget_failures)
                )
            # Fixed-point packaged error uses the existing normalized surface
            # metric and source cell diagonal, not a second geometry metric.
            geom_error = float(quality_report["surface"]["max"]) * cell_source.bbox_diagonal()
            error_fx = _fx16(geom_error, "LOD geometric error")
            payload = _mesh_payload(mesh, key, cell_size)
            metrics = _texture_metrics(source, cell_source, validation)
            metrics.update({
                "name": targets[level_index].name,
                "source_sha256": cell_source_hash,
                "requested_triangles": target,
                "faces": len(mesh.triangles),
                "vertices": len(mesh.positions),
                "source_triangles": len(cell_source.triangles),
                "source_triangle_coverage": len(cell_source.source_triangle_ids) / len(source.triangles),
                "geometric_error": geom_error,
                "geometric_error_fx16": error_fx,
                "view_error": quality_report["silhouette"],
                "fixed_camera_view_error": _fixed_camera_view_error(
                    cell_source, mesh, size=96),
                "quality_passed": True,
                "budget_failures": budget_failures,
                "budget_passed": not budget_failures,
                "payload_bytes": len(payload),
            })
            records.append({"v": len(mesh.positions), "f": len(mesh.triangles),
                            "error": error_fx, "texture_bytes": metrics["texture_bytes"],
                            "payload": payload})
            variants_report.append(metrics)
        cell_record = {"key": key, "bounds_fx16": bounds_fx, "levels": records}
        cell_records.append(cell_record)
        cell_reports.append({
            "cell": list(key),
            "source_sha256": cell_source_hash,
            "aabb_local": [v / 65536.0 for v in bounds_fx],
            "source_triangles": len(cell_source.triangles),
            "source_area": area_by_cell,
            "source_triangle_ids": cell_source.source_triangle_ids,
            "levels": variants_report,
        })

    # Directory size is fixed before assigning payload offsets. Each level is
    # packed as signed 16.16 cell-relative xyz/UV plus BE u16 triangle/material.
    directory_bytes = sum(CELL.size + LEVEL.size * len(c["levels"]) for c in cell_records)
    cursor = HEADER.size + directory_bytes
    for cell in cell_records:
        for level in cell["levels"]:
            level["offset"] = cursor
            cursor += len(level["payload"])
    total_bytes = cursor
    if total_bytes > max_package_bytes:
        raise GltfError(f"world_hlod: package is {total_bytes} bytes, limit is {max_package_bytes}")

    out = bytearray(HEADER.pack(MAGIC, VERSION, len(cell_records), total_bytes,
                                bytes.fromhex(source_hash)))
    for cell in cell_records:
        out.extend(CELL.pack(*cell["key"], *cell["bounds_fx16"], len(cell["levels"])))
        for level in cell["levels"]:
            out.extend(LEVEL.pack(level["v"], level["f"], level["error"],
                                  level["texture_bytes"], level["offset"],
                                  len(level["payload"])))
    for cell in cell_records:
        for level in cell["levels"]:
            if len(out) != level["offset"]:
                raise GltfError("world_hlod: internal payload offset accounting mismatch")
            out.extend(level["payload"])
    if len(out) != total_bytes:
        raise GltfError(f"world_hlod: exact byte accounting mismatch {len(out)} != {total_bytes}")

    total_partitioned_area = sum(cell["source_area"] for cell in cell_reports)
    report = {
        "format": "WLH1",
        "version": VERSION,
        "source_sha256": source_hash,
        "package_bytes": len(out),
        "header_bytes": HEADER.size,
        "directory_bytes": directory_bytes,
        "payload_bytes": len(out) - HEADER.size - directory_bytes,
        "cell_size": cell_size,
        "validation_camera_directions": [list(v) for v in
                                         silhouette_mod.fibonacci_views(silhouette_views)],
        "cell_count": len(cell_reports),
        "source_triangles": len(source.triangles),
        "partitioned_source_triangle_fragments": sum(c["source_triangles"] for c in cell_reports),
        "source_area_coverage": (total_partitioned_area / source_area if source_area else 1.0),
        "source_triangle_coverage": len(source_ids_covered) / len(source.triangles),
        "global_texture_bytes": validation["texture_bytes"],
        "largest_vdp1_sprite_extent": validation["largest_texture_extent"],
        "palette_validation": {
            "material_indices_valid": validation["material_indices_valid"],
            "texture_references_valid": validation["texture_references_valid"],
            "palette_entries_max": validation["palette_entries"],
        },
        "uv_validation": {"finite": validation["uvs_finite"]},
        "protected_regions": [r.name for r in regions],
        "cells": cell_reports,
    }
    return bytes(out), report


def demo_source() -> SourceModel:
    """Original small castle facade scene with a door, two windows and towers."""
    vertices: list[tuple[float, float, float]] = []
    uvs: list[tuple[float, float]] = []
    triangles: list[tuple[int, int, int]] = []
    materials = [
        {"name": "stone", "texture_index": 0, "palette_entries": 16},
        {"name": "trim", "texture_index": 1, "palette_entries": 16},
        {"name": "roof", "palette_entries": 8},
        {"name": "door", "palette_entries": 8},
        {"name": "window", "palette_entries": 8},
    ]
    tri_materials: list[int] = []

    def quad(points, material, uv_box=(0.0, 0.0, 1.0, 1.0), divisions=(1, 1),
             reverse=False):
        p00, p10, p11, p01 = points
        u0, v0, u1, v1 = uv_box
        nx, ny = divisions
        base = len(vertices)
        for row in range(ny + 1):
            v = row / ny
            for col in range(nx + 1):
                u = col / nx
                left = tuple(p00[a] + (p01[a] - p00[a]) * v for a in range(3))
                right = tuple(p10[a] + (p11[a] - p10[a]) * v for a in range(3))
                vertices.append(tuple(left[a] + (right[a] - left[a]) * u for a in range(3)))
                uvs.append((u0 + (u1 - u0) * u, v0 + (v1 - v0) * v))
        for row in range(ny):
            for col in range(nx):
                a = base + row * (nx + 1) + col
                b, d = a + 1, a + nx + 1
                c = d + 1
                if reverse:
                    triangles.extend(((a, c, b), (a, d, c)))
                else:
                    triangles.extend(((a, b, c), (a, c, d)))
                tri_materials.extend((material, material))

    def triangle(points, material, reverse=False):
        base = len(vertices)
        vertices.extend(points)
        uvs.extend(((0.0, 0.0), (1.0, 0.0), (0.5, 1.0)))
        triangles.append((base, base + 2, base + 1) if reverse else
                         (base, base + 1, base + 2))
        tri_materials.append(material)

    # Front wall divided around openings. The gaps remain real silhouette
    # holes; trim strips are separately protected annotations.
    z = 0.0
    for x0, x1 in ((-12.0, -7.0), (-3.0, 3.0), (7.0, 12.0)):
        quad(((x0, 0, z), (x1, 0, z), (x1, 14, z), (x0, 14, z)), 0,
             divisions=(3, 4), reverse=True)
    # Door header and window ledges close the remaining wall regions.
    quad(((-7, 0, z), (7, 0, z), (7, 5, z), (-7, 5, z)), 0,
         divisions=(4, 2), reverse=True)
    quad(((-7, 9, z), (7, 9, z), (7, 14, z), (-7, 14, z)), 0,
         divisions=(4, 2), reverse=True)
    # Door panel and window glass sit recessed behind openings.
    quad(((-2.8, 0.2, 0.2), (2.8, 0.2, 0.2), (2.8, 4.8, 0.2), (-2.8, 4.8, 0.2)), 3,
         reverse=True)
    for x0, x1 in ((-6.6, -3.4), (3.4, 6.6)):
        quad(((x0, 6.0, 0.2), (x1, 6.0, 0.2), (x1, 8.8, 0.2), (x0, 8.8, 0.2)), 4,
             reverse=True)
    # Original frame/trim around doors and windows.
    for x in (-3.1, 3.1):
        quad(((x - .12, 0, -.1), (x + .12, 0, -.1),
              (x + .12, 5.2, -.1), (x - .12, 5.2, -.1)), 1, reverse=True)
    for x0, x1 in ((-6.9, -3.1), (3.1, 6.9)):
        for y in (5.7, 8.9):
            quad(((x0, y, -.1), (x1, y, -.1),
                  (x1, y + .14, -.1), (x0, y + .14, -.1)), 1, reverse=True)
    # Side walls and pitched roof provide an oblique silhouette.
    quad(((-12, 0, 0), (-12, 0, 10), (-12, 14, 10), (-12, 14, 0)), 0)
    quad(((12, 0, 0), (12, 0, 10), (12, 14, 10), (12, 14, 0)), 0, reverse=True)
    triangle(((-14, 14, -1), (14, 14, -1), (0, 20, -1)), 2, reverse=True)
    # The runtime demo supplies the ground as VDP2 rotation-screen imagery;
    # keep this package to VDP1 castle geometry so the two layers compose
    # without a giant overlapping VDP1 floor.
    texels0 = [(96, 88, 80, 255)] * (8 * 8)
    texels1 = [(156, 140, 110, 255)] * (8 * 8)
    from .model import SourceTexture
    return SourceModel(vertices=vertices, uvs=uvs, triangles=triangles,
                       tri_materials=tri_materials, materials=materials,
                       textures=[SourceTexture(8, 8, texels0),
                                 SourceTexture(8, 8, texels1)])


def build_demo_package() -> tuple[bytes, dict]:
    source = demo_source()
    return build_package(
        source,
        cell_size=16.0,
        lod_targets=(LodTarget("lod0", 1000), LodTarget("lod1", 28),
                     LodTarget("lod2", 18)),
        quality="aggressive",
        protected_regions=(
            {"name": "door-frame", "minimum": (-3.3, 0.0, -0.2),
             "maximum": (3.3, 5.3, 0.0)},
            {"name": "window-frames", "minimum": (-7.0, 5.5, -0.2),
             "maximum": (7.0, 9.2, 0.0)},
        ),
        silhouette_views=12,
    )
