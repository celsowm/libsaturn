#!/usr/bin/env python3
"""Generic OBJ/MTL textured-model importer for LibSaturn.

Reads a Wavefront OBJ with its MTL and image texture(s), bakes every face
into a canonical VDP1-compatible rectangular texture, deduplicates the baked
payloads, builds one shared indexed8 palette, and emits a deterministic C/H
compiled-model asset for the SH-2 build.

Pipeline (source UVs never reach the runtime)::

    OBJ + MTL + PNG
          |
          | tools/import_model.py  (this tool, host-side)
          v
    generated C/H model
          |
          | upload once (sat_model_upload_textures)
          v
    LibSaturn textured mesh (sat_scene_t model/instance path)
          |
          v
    VDP1 distorted sprites

Face convention (LibSaturn, authoritative)::

    A = top-left, B = top-right, C = bottom-right, D = bottom-left
    normal = cross(D - A, B - A)

OBJ faces wind counter-clockwise from the front; LibSaturn faces wind
clockwise from the front (A->B->C->D as above). The importer therefore
reverses OBJ winding by default (see --reverse-winding to disable).

Triangles become degenerate quads with the last corner duplicated, and the
duplicated geometry corner carries the duplicated UV so the canonical
rectangle collapses together with the geometric quad on the VDP1.
"""

from __future__ import annotations

import argparse
import hashlib
import importlib.metadata
import json
import math
import os
import sys
import tempfile
from collections import Counter
from dataclasses import dataclass, field
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from model_pipeline import animation as anim_eval
from model_pipeline import emit_c as emit_anim
from model_pipeline import face_colors as face_color_mod
from model_pipeline import gltf as gltf_mod
from model_pipeline import lod as lod_mod
from model_pipeline import metrics as metrics_mod
from model_pipeline import model as srcmodel
from model_pipeline import pose_bake
from model_pipeline import quad_merge as quad_merge_mod
from model_pipeline import saturn_profile as saturn_profile_mod
from model_pipeline import silhouette as sil_mod
from model_pipeline import simplification as simp_mod
from saturn_asset_common import (
    asset_header_guard,
    asset_symbol_prefix,
    format_byte_array,
    format_int_array,
    format_ushort_array,
    format_word_array,
    rgb888_to_rgb555,
    sanitize_identifier,
)

VDP1_MAX_TEXTURE_WIDTH = 504
VDP1_MAX_TEXTURE_HEIGHT = 255
VDP1_VRAM_BYTES = 512 * 1024
VDP1_COMMAND_AREA_BYTES = 64 * 1024
FX16_ONE = 65536


class ImportError(Exception):
    pass


IMPORT_SIGNATURE_SCHEMA = 1


def _sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def _canonical_path(path: Path) -> str:
    try:
        return str(path.resolve())
    except OSError:
        return str(path.absolute())


def _referenced_input_paths(input_path: Path) -> list[Path]:
    """Find local files consumed in addition to the primary model input.

    This is deliberately best-effort: malformed inputs still reach the normal
    importer, which owns the user-facing diagnostic. Existing external assets
    are hashed; missing ones are recorded as missing in the signature.
    """
    paths: list[Path] = []
    suffix = input_path.suffix.lower()
    try:
        if suffix == ".obj":
            model = parse_obj(input_path)
            for mtl_name in model.mtllibs:
                mtl_path = Path(mtl_name)
                if not mtl_path.is_absolute():
                    mtl_path = input_path.parent / mtl_path
                paths.append(mtl_path)
                try:
                    materials = parse_mtl(mtl_path)
                except ImportError:
                    continue
                for material in materials.values():
                    texture_name = material.get("map_Kd")
                    if texture_name:
                        texture_path = Path(texture_name)
                        if not texture_path.is_absolute():
                            texture_path = input_path.parent / texture_path
                        paths.append(texture_path)
        elif suffix in (".glb", ".gltf"):
            if suffix == ".glb":
                doc = gltf_mod.parse_glb(input_path).json
            else:
                doc = json.loads(input_path.read_text(encoding="utf-8"))
            for group in ("buffers", "images"):
                for item in doc.get(group, []):
                    uri = item.get("uri") if isinstance(item, dict) else None
                    if not isinstance(uri, str) or uri.startswith("data:"):
                        continue
                    referenced = Path(uri)
                    if not referenced.is_absolute():
                        referenced = input_path.parent / referenced
                    paths.append(referenced)
    except (ImportError, OSError, ValueError, TypeError, KeyError, gltf_mod.GltfError):
        pass

    unique: dict[str, Path] = {}
    for path in paths:
        unique.setdefault(_canonical_path(path), path)
    return [unique[key] for key in sorted(unique)]


def _import_signature(args: argparse.Namespace) -> dict:
    input_path = Path(args.input)
    dependencies = [input_path, *_referenced_input_paths(input_path)]
    input_records = []
    for path in dependencies:
        record = {"path": _canonical_path(path)}
        if path.is_file():
            record["sha256"] = _sha256_file(path)
        else:
            record["missing"] = True
        input_records.append(record)

    tools_dir = Path(__file__).resolve().parent
    tool_paths = [tools_dir / "import_model.py", tools_dir / "saturn_asset_common.py"]
    tool_paths.extend(sorted((tools_dir / "model_pipeline").glob("*.py")))
    tool_records = [
        {"path": _canonical_path(path), "sha256": _sha256_file(path)}
        for path in tool_paths if path.is_file()
    ]
    try:
        pillow_version = importlib.metadata.version("Pillow")
    except importlib.metadata.PackageNotFoundError:
        pillow_version = "not-installed"

    options = {
        key: value for key, value in vars(args).items()
        if key not in ("incremental", "force_import", "signature_only", "signature_file")
    }
    for key in ("input", "out_prefix", "report"):
        value = options.get(key)
        if value is not None:
            options[key] = _canonical_path(Path(value))

    payload = {
        "schema_version": IMPORT_SIGNATURE_SCHEMA,
        "options": options,
        "inputs": input_records,
        "tools": tool_records,
        "runtime": {"python": sys.version, "pillow": pillow_version},
    }
    canonical = json.dumps(payload, sort_keys=True, separators=(",", ":"))
    payload["signature"] = hashlib.sha256(canonical.encode("utf-8")).hexdigest()
    return payload


def _atomic_write_json(path: Path, payload: dict, preserve_if_equal: bool = False) -> None:
    encoded = (json.dumps(payload, indent=2, sort_keys=True) + "\n").encode("utf-8")
    if preserve_if_equal:
        try:
            if path.read_bytes() == encoded:
                return
        except OSError:
            pass
    path.parent.mkdir(parents=True, exist_ok=True)
    temp_name = None
    try:
        with tempfile.NamedTemporaryFile(
            mode="wb", dir=path.parent, prefix=path.name + ".", suffix=".tmp",
            delete=False,
        ) as stream:
            temp_name = stream.name
            stream.write(encoded)
        os.replace(temp_name, path)
    finally:
        if temp_name and os.path.exists(temp_name):
            os.unlink(temp_name)


def _model_output_paths(args: argparse.Namespace) -> dict[str, Path]:
    prefix = str(args.out_prefix)
    outputs = {"source": Path(prefix + ".c"), "header": Path(prefix + ".h")}
    if args.report and Path(args.input).suffix.lower() in (".glb", ".gltf"):
        outputs["report"] = Path(args.report)
    return outputs


def _cache_manifest_path(args: argparse.Namespace) -> Path:
    return Path(str(args.out_prefix) + ".import.json")


def _incremental_cache_hit(args: argparse.Namespace, signature: dict) -> bool:
    manifest_path = _cache_manifest_path(args)
    try:
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    except (OSError, ValueError):
        return False
    if not isinstance(manifest, dict):
        return False
    if (manifest.get("schema_version") != IMPORT_SIGNATURE_SCHEMA or
            manifest.get("signature") != signature["signature"]):
        return False
    recorded_outputs = manifest.get("outputs", {})
    if not isinstance(recorded_outputs, dict):
        return False
    for role, path in _model_output_paths(args).items():
        recorded = recorded_outputs.get(role, {})
        if not isinstance(recorded, dict):
            return False
        if not path.is_file() or recorded.get("path") != _canonical_path(path):
            return False
        try:
            if recorded.get("sha256") != _sha256_file(path):
                return False
        except OSError:
            return False
    return True


def _write_incremental_manifest(args: argparse.Namespace, signature: dict) -> None:
    outputs = {}
    for role, path in _model_output_paths(args).items():
        outputs[role] = {"path": _canonical_path(path), "sha256": _sha256_file(path)}
    manifest = {
        "schema_version": IMPORT_SIGNATURE_SCHEMA,
        "signature": signature["signature"],
        "outputs": outputs,
    }
    _atomic_write_json(_cache_manifest_path(args), manifest)


# ----------------------------------------------------------------------
# OBJ / MTL parsing
# ----------------------------------------------------------------------

@dataclass
class ObjModel:
    vertices: list[tuple[float, float, float]] = field(default_factory=list)
    uvs: list[tuple[float, float]] = field(default_factory=list)
    # Each face: list of (vertex_index, uv_index|None), material name.
    faces: list[dict] = field(default_factory=list)
    mtllibs: list[str] = field(default_factory=list)
    material_count: int = 0


def _resolve_index(raw: int, count: int, what: str, lineno: int, path: Path) -> int:
    if raw > 0:
        idx = raw - 1
    elif raw < 0:
        idx = count + raw
    else:
        raise ImportError(f"{path}:{lineno}: {what} index 0 is invalid (OBJ indices are 1-based)")
    if idx < 0 or idx >= count:
        raise ImportError(
            f"{path}:{lineno}: {what} index {raw} out of range ({count} available)"
        )
    return idx


def parse_obj(path: Path) -> ObjModel:
    try:
        text = path.read_text(encoding="utf-8", errors="strict")
    except FileNotFoundError:
        raise ImportError(f"OBJ not found: {path}")
    except OSError as exc:
        raise ImportError(f"Cannot read OBJ {path}: {exc}")

    model = ObjModel()
    materials_seen: set[str] = set()
    current_mtl: str | None = None
    source_tris = 0
    source_quads = 0

    for lineno, raw_line in enumerate(text.splitlines(), start=1):
        line = raw_line.strip()
        if not line or line.startswith("#"):
            continue
        parts = line.split()
        tag = parts[0]
        if tag == "v":
            if len(parts) < 4:
                raise ImportError(f"{path}:{lineno}: malformed 'v' line: {raw_line.strip()}")
            try:
                x, y, z = float(parts[1]), float(parts[2]), float(parts[3])
            except ValueError:
                raise ImportError(f"{path}:{lineno}: malformed vertex coordinates: {raw_line.strip()}")
            model.vertices.append((x, y, z))
        elif tag == "vt":
            if len(parts) < 3:
                raise ImportError(f"{path}:{lineno}: malformed 'vt' line: {raw_line.strip()}")
            try:
                u, v = float(parts[1]), float(parts[2])
            except ValueError:
                raise ImportError(f"{path}:{lineno}: malformed UV coordinates: {raw_line.strip()}")
            model.uvs.append((u, v))
        elif tag == "vn":
            continue  # parsed/ignored: LibSaturn derives face normals from geometry
        elif tag == "f":
            if len(parts) < 4:
                raise ImportError(f"{path}:{lineno}: malformed 'f' line (fewer than 3 vertices)")
            verts: list[tuple[int, int | None]] = []
            for tok in parts[1:]:
                fields = tok.split("/")
                try:
                    vi_raw = int(fields[0])
                except ValueError:
                    raise ImportError(f"{path}:{lineno}: malformed face index: {tok}")
                vi = _resolve_index(vi_raw, len(model.vertices), "vertex", lineno, path)
                vti: int | None = None
                if len(fields) >= 2 and fields[1] != "":
                    try:
                        vti_raw = int(fields[1])
                    except ValueError:
                        raise ImportError(f"{path}:{lineno}: malformed UV index: {tok}")
                    if not model.uvs:
                        raise ImportError(
                            f"{path}:{lineno}: face references UVs but no 'vt' entries exist"
                        )
                    vti = _resolve_index(vti_raw, len(model.uvs), "UV", lineno, path)
                verts.append((vi, vti))
            n = len(verts)
            if n == 3:
                source_tris += 1
                model.faces.append({"verts": verts, "mtl": current_mtl, "lineno": lineno})
            elif n == 4:
                source_quads += 1
                model.faces.append({"verts": verts, "mtl": current_mtl, "lineno": lineno})
            elif n > 4:
                # Deterministic fan triangulation: (0, i, i+1).
                source_tris += n - 2
                for i in range(1, n - 1):
                    tri = [verts[0], verts[i], verts[i + 1]]
                    model.faces.append({"verts": tri, "mtl": current_mtl, "lineno": lineno})
            else:
                raise ImportError(f"{path}:{lineno}: face with {n} vertices is not a polygon")
        elif tag == "mtllib":
            if len(parts) < 2:
                raise ImportError(f"{path}:{lineno}: malformed 'mtllib' line")
            model.mtllibs.append(parts[1])
        elif tag == "usemtl":
            current_mtl = parts[1] if len(parts) > 1 else None
            if current_mtl is not None and current_mtl not in materials_seen:
                materials_seen.add(current_mtl)
        elif tag in ("o", "g", "s", "mg"):
            continue
        else:
            continue

    model.material_count = len(materials_seen)
    model.faces = model.faces  # stable source order
    # Stash source stats for reporting without extra passes.
    model.source_tris = source_tris  # type: ignore[attr-defined]
    model.source_quads = source_quads  # type: ignore[attr-defined]
    return model


def parse_mtl(path: Path) -> dict[str, dict]:
    try:
        text = path.read_text(encoding="utf-8", errors="strict")
    except FileNotFoundError:
        raise ImportError(f"MTL not found: {path}")
    except OSError as exc:
        raise ImportError(f"Cannot read MTL {path}: {exc}")
    materials: dict[str, dict] = {}
    current: str | None = None
    for lineno, raw_line in enumerate(text.splitlines(), start=1):
        line = raw_line.strip()
        if not line or line.startswith("#"):
            continue
        parts = line.split()
        if parts[0] == "newmtl" and len(parts) > 1:
            current = parts[1]
            materials[current] = {}
        elif parts[0] == "map_Kd" and current is not None:
            # map_Kd may carry options before the filename; the filename is last.
            materials[current]["map_Kd"] = parts[-1]
    return materials


def resolve_material_texture(
    mtl_name: str | None,
    materials: dict[str, dict],
    obj_dir: Path,
    obj_path: Path,
) -> Path:
    if mtl_name is None:
        raise ImportError(
            f"{obj_path}: face uses no material (missing 'usemtl' before 'f'); "
            "assign a material with map_Kd in the MTL"
        )
    if mtl_name not in materials:
        raise ImportError(
            f"{obj_path}: material '{mtl_name}' not found in MTL "
            f"(have: {sorted(materials) or 'none'})"
        )
    entry = materials[mtl_name]
    if "map_Kd" not in entry:
        raise ImportError(
            f"{obj_path}: material '{mtl_name}' has no map_Kd texture"
        )
    tex = Path(entry["map_Kd"])
    candidate = tex if tex.is_absolute() else (obj_dir / tex)
    if not candidate.exists():
        raise ImportError(
            f"{obj_path}: texture '{entry['map_Kd']}' for material '{mtl_name}' "
            f"not found (looked at {candidate})"
        )
    return candidate


# ----------------------------------------------------------------------
# Images
# ----------------------------------------------------------------------

def load_rgba_image(path: Path) -> tuple[int, int, list[tuple[int, int, int, int]]]:
    try:
        from PIL import Image
    except ModuleNotFoundError as exc:
        raise ImportError("Image support needs Pillow: pip install pillow") from exc
    try:
        img = Image.open(path)
    except FileNotFoundError:
        raise ImportError(f"Texture image not found: {path}")
    except OSError as exc:
        raise ImportError(f"Cannot open texture image {path}: {exc}")
    img = img.convert("RGBA")
    w, h = img.size
    if w <= 0 or h <= 0:
        raise ImportError(f"Texture image {path} has invalid size {w}x{h}")
    get_flat = getattr(img, "get_flattened_data", None)
    data = list(get_flat()) if callable(get_flat) else list(img.getdata())
    pixels = [(r, g, b, a) for (r, g, b, a) in data]
    return w, h, pixels


# ----------------------------------------------------------------------
# Canonicalization and winding
# ----------------------------------------------------------------------

@dataclass
class BakedFaceInput:
    # Quad geometry indices + UVs in LibSaturn A/B/C/D order.
    vert_ids: tuple[int, int, int, int]
    uvs: tuple[tuple[float, float], tuple[float, float], tuple[float, float], tuple[float, float]]
    mtl: str | None
    lineno: int
    is_triangle: bool


def canonicalize_faces(
    model: ObjModel,
    reverse_winding: bool,
    flip_x: bool,
    flip_y: bool,
    flip_z: bool,
) -> tuple[list[tuple[float, float, float]], list[BakedFaceInput]]:
    verts = list(model.vertices)
    if flip_x or flip_y or flip_z:
        flipped = []
        for (x, y, z) in verts:
            if flip_x:
                x = -x
            if flip_y:
                y = -y
            if flip_z:
                z = -z
            flipped.append((x, y, z))
        verts = flipped
        # Mirroring across an odd number of axes inverts orientation, so the
        # winding reversal toggles to keep outward normals outward.
        if (int(bool(flip_x)) + int(bool(flip_y)) + int(bool(flip_z))) % 2 == 1:
            reverse_winding = not reverse_winding

    out: list[BakedFaceInput] = []
    for face in model.faces:
        items = face["verts"]
        mtl = face["mtl"]
        lineno = face["lineno"]
        for _, vti in items:
            if vti is None:
                raise ImportError(
                    f"Face at line {lineno} is missing UV indices "
                    "(all faces need v/vt for textured import)"
                )
        if len(items) == 3:
            (a, au), (b, bu), (c, cu) = items
            auv = model.uvs[au]  # type: ignore[index]
            buv = model.uvs[bu]  # type: ignore[index]
            cuv = model.uvs[cu]  # type: ignore[index]
            if reverse_winding:
                # Direct OBJ order (CCW) would wind inward under
                # cross(D-A, B-A); keep OBJ order for --reverse-winding.
                out.append(BakedFaceInput((a, b, c, c), (auv, buv, cuv, cuv), mtl, lineno, True))
            else:
                # Default: reverse to clockwise (outward): A=v0, B=v2, C=v1.
                out.append(BakedFaceInput((a, c, b, b), (auv, cuv, buv, buv), mtl, lineno, True))
        elif len(items) == 4:
            (a, au), (b, bu), (c, cu), (d, du) = items
            auv = model.uvs[au]  # type: ignore[index]
            buv = model.uvs[bu]  # type: ignore[index]
            cuv = model.uvs[cu]  # type: ignore[index]
            duv = model.uvs[du]  # type: ignore[index]
            if reverse_winding:
                out.append(
                    BakedFaceInput((a, b, c, d), (auv, buv, cuv, duv), mtl, lineno, False)
                )
            else:
                # Reverse to clockwise: A=v0, B=v3, C=v2, D=v1.
                out.append(
                    BakedFaceInput((a, d, c, b), (auv, duv, cuv, buv), mtl, lineno, False)
                )
        else:  # pragma: no cover - fan triangulation above guarantees 3/4
            raise ImportError(f"Face at line {lineno} has {len(items)} vertices after triangulation")
    return verts, out


def apply_scale(
    verts: list[tuple[float, float, float]], scale: float
) -> list[tuple[float, float, float]]:
    if scale == 1.0:
        return verts
    return [(x * scale, y * scale, z * scale) for (x, y, z) in verts]


def float_to_fx16(value: float) -> int:
    return int(math.floor(value * FX16_ONE + 0.5))


# ----------------------------------------------------------------------
# Baking
# ----------------------------------------------------------------------

def uv_to_pixel(uv: tuple[float, float], img_w: int, img_h: int) -> tuple[float, float]:
    u, v = uv
    # OBJ v=0 is the bottom of the image; image row 0 is the top.
    x = u * (img_w - 1)
    y = (1.0 - v) * (img_h - 1)
    return x, y


def estimate_face_size(
    uvs: tuple[tuple[float, float], tuple[float, float], tuple[float, float], tuple[float, float]],
    img_w: int,
    img_h: int,
    texture_scale: float,
) -> tuple[int, int]:
    (au, av), (bu, bv), (cu, cv), (du, dv) = uvs
    ax, ay = au * img_w, (1.0 - av) * img_h
    bx, by = bu * img_w, (1.0 - bv) * img_h
    cx, cy = cu * img_w, (1.0 - cv) * img_h
    dx, dy = du * img_w, (1.0 - dv) * img_h
    top = math.hypot(bx - ax, by - ay)
    bottom = math.hypot(cx - dx, cy - dy)
    left = math.hypot(dx - ax, dy - ay)
    right = math.hypot(cx - bx, cy - by)
    w = max(top, bottom) * texture_scale
    h = max(left, right) * texture_scale
    w = max(1, int(round(w)))
    h = max(1, int(round(h)))
    return w, h


def conform_size(
    est_w: int,
    est_h: int,
    max_w: int,
    max_h: int,
    mtl: str | None,
    lineno: int,
) -> tuple[int, int]:
    if max_w < 8 or max_w > VDP1_MAX_TEXTURE_WIDTH or (max_w & 7) != 0:
        raise ImportError(
            f"--max-texture-width must be a multiple of 8 in 8..{VDP1_MAX_TEXTURE_WIDTH} "
            f"(got {max_w})"
        )
    if max_h < 1 or max_h > VDP1_MAX_TEXTURE_HEIGHT:
        raise ImportError(
            f"--max-texture-height must be in 1..{VDP1_MAX_TEXTURE_HEIGHT} (got {max_h})"
        )
    # Resample the same UV domain across the legal aligned width instead of
    # padding with unused columns (the VDP1 maps the whole stored width).
    aligned_w = ((max(est_w, 1) + 7) // 8) * 8
    if aligned_w < 8:
        aligned_w = 8
    out_h = max(est_h, 1)
    if aligned_w > max_w or out_h > max_h:
        raise ImportError(
            f"Face (material '{mtl}', OBJ line {lineno}) needs {aligned_w}x{out_h} "
            f"but the limit is {max_w}x{max_h}; raise --max-texture-width/height "
            f"(hardware max {VDP1_MAX_TEXTURE_WIDTH}x{VDP1_MAX_TEXTURE_HEIGHT}) "
            f"or lower --texture-scale"
        )
    return aligned_w, out_h


def bake_face_rgba(
    uvs: tuple[tuple[float, float], tuple[float, float], tuple[float, float], tuple[float, float]],
    img_w: int,
    img_h: int,
    img_pixels: list[tuple[int, int, int, int]],
    out_w: int,
    out_h: int,
    sampling: str,
) -> list[tuple[int, int, int, int]]:
    if sampling == "area":
        return _bake_face_area(uvs, img_w, img_h, img_pixels, out_w, out_h)
    if sampling != "nearest":
        raise ImportError(f"Unknown --sampling '{sampling}' (supported: nearest, area)")
    (au, av), (bu, bv), (cu, cv), (du, dv) = uvs
    out: list[tuple[int, int, int, int]] = []
    for y in range(out_h):
        t = (y + 0.5) / out_h
        for x in range(out_w):
            s = (x + 0.5) / out_w
            # Bilinear parameterization over the canonical rectangle:
            # top = lerp(A, B, s); bottom = lerp(D, C, s); uv = lerp(top, bottom, t).
            top_u = au + (bu - au) * s
            top_v = av + (bv - av) * s
            bot_u = du + (cu - du) * s
            bot_v = dv + (cv - dv) * s
            u = top_u + (bot_u - top_u) * t
            v = top_v + (bot_v - top_v) * t
            # Clamp to the intended source bounds (no atlas bleeding).
            if u < 0.0:
                u = 0.0
            elif u > 1.0:
                u = 1.0
            if v < 0.0:
                v = 0.0
            elif v > 1.0:
                v = 1.0
            fx = u * (img_w - 1)
            fy = (1.0 - v) * (img_h - 1)
            ix = int(math.floor(fx + 0.5))
            iy = int(math.floor(fy + 0.5))
            if ix < 0:
                ix = 0
            elif ix >= img_w:
                ix = img_w - 1
            if iy < 0:
                iy = 0
            elif iy >= img_h:
                iy = img_h - 1
            out.append(img_pixels[iy * img_w + ix])
    return out


_LINEAR_CACHE: dict[int, tuple] = {}


def _linear_image(img_w: int, img_h: int, img_pixels) -> "np.ndarray":
    import numpy as np

    key = id(img_pixels)
    hit = _LINEAR_CACHE.get(key)
    if hit is None or hit[0] is not img_pixels:
        srgb = np.asarray(img_pixels, dtype=np.float64).reshape(img_h, img_w, 4)
        lin = srgb.copy()
        lin[..., :3] = (srgb[..., :3] / 255.0) ** 2.2
        lin[..., 3] /= 255.0
        hit = (img_pixels, lin)
        _LINEAR_CACHE[key] = hit
    return hit[1]


def _bake_face_area(uvs, img_w, img_h, img_pixels, out_w, out_h):
    """Box-filtered bake: each output texel averages the source area it covers.

    The VDP1 samples textures nearest-neighbour with no mipmapping, so a face
    baked at more texels than it covers on screen shimmers into speckle
    noise. Baking at the on-screen size (see --texel-extent) with this filter
    is the offline equivalent of a mip level. Samples are bilinear source
    fetches on a k x k grid per texel, averaged in linear light.
    """
    import numpy as np

    lin = _linear_image(img_w, img_h, img_pixels)
    (au, av), (bu, bv), (cu, cv), (du, dv) = uvs
    src_w, src_h = estimate_face_size(uvs, img_w, img_h, 1.0)
    ratio = max(src_w / out_w, src_h / out_h, 1.0)
    k = int(min(8, max(2, math.ceil(ratio * 1.5))))
    sub = (np.arange(k) + 0.5) / k
    s = ((np.arange(out_w)[:, None] + sub[None, :]) / out_w).reshape(-1)
    t = ((np.arange(out_h)[:, None] + sub[None, :]) / out_h).reshape(-1)
    S, T = np.meshgrid(s, t)
    top_u = au + (bu - au) * S
    top_v = av + (bv - av) * S
    bot_u = du + (cu - du) * S
    bot_v = dv + (cv - dv) * S
    u = np.clip(top_u + (bot_u - top_u) * T, 0.0, 1.0)
    v = np.clip(top_v + (bot_v - top_v) * T, 0.0, 1.0)
    fx = u * (img_w - 1)
    fy = (1.0 - v) * (img_h - 1)
    x0 = np.clip(np.floor(fx).astype(int), 0, img_w - 1)
    y0 = np.clip(np.floor(fy).astype(int), 0, img_h - 1)
    x1 = np.minimum(x0 + 1, img_w - 1)
    y1 = np.minimum(y0 + 1, img_h - 1)
    wx = (fx - x0)[..., None]
    wy = (fy - y0)[..., None]
    val = ((lin[y0, x0] * (1 - wx) + lin[y0, x1] * wx) * (1 - wy)
           + (lin[y1, x0] * (1 - wx) + lin[y1, x1] * wx) * wy)
    val = val.reshape(out_h, k, out_w, k, 4).mean(axis=(1, 3))
    rgb = np.clip(np.round((val[..., :3] ** (1 / 2.2)) * 255.0), 0, 255).astype(int)
    alpha = np.clip(np.round(val[..., 3] * 255.0), 0, 255).astype(int)
    out = np.concatenate([rgb, alpha[..., None]], axis=-1).reshape(-1, 4)
    return [tuple(int(c) for c in px) for px in out]


# ----------------------------------------------------------------------
# Palette
# ----------------------------------------------------------------------

def build_shared_palette(
    baked_rgba: list[list[tuple[int, int, int, int]]],
) -> tuple[list[tuple[int, int, int]], bool, list[int]]:
    """Returns (palette_rgb888, has_transparency, per-face opaque flags).

    palette index 0 is reserved for transparent pixels when any baked face
    has transparency; opaque colors fill the remaining entries.
    """
    has_transparency = any(a < 128 for face in baked_rgba for (_, _, _, a) in face)
    limit = 255 if has_transparency else 256

    opaque: list[tuple[int, int, int]] = []
    for face in baked_rgba:
        for (r, g, b, a) in face:
            if a >= 128:
                opaque.append((r, g, b))
    if not opaque:
        # Fully transparent model: single transparent entry.
        return [(0, 0, 0)], True, [0] * len(baked_rgba)

    counts = Counter(opaque)
    distinct = len(counts)
    if distinct <= limit:
        # Deterministic: most frequent first, ties broken by RGB.
        ordered = sorted(counts.items(), key=lambda kv: (-kv[1], kv[0]))
        entries = [rgb for (rgb, _) in ordered]
    else:
        entries = _quantize_colors(opaque, limit)

    if has_transparency:
        palette = [(0, 0, 0)] + entries[:255]
    else:
        palette = entries[:256]
    return palette, has_transparency, [0] * len(baked_rgba)


def _quantize_colors(
    opaque: list[tuple[int, int, int]], limit: int
) -> list[tuple[int, int, int]]:
    from PIL import Image

    n = len(opaque)
    w = min(n, 1024)
    h = (n + w - 1) // w
    img = Image.new("RGB", (w, h), (0, 0, 0))
    img.putdata(opaque + [(0, 0, 0)] * (w * h - n))
    q = img.quantize(colors=limit, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE)
    pal = q.getpalette() or []
    entries: list[tuple[int, int, int]] = []
    for i in range(limit):
        r = pal[i * 3] if i * 3 < len(pal) else 0
        g = pal[i * 3 + 1] if i * 3 + 1 < len(pal) else 0
        b = pal[i * 3 + 2] if i * 3 + 2 < len(pal) else 0
        entries.append((r, g, b))
    # Drop trailing duplicates while keeping order deterministic.
    seen: set[tuple[int, int, int]] = set()
    unique: list[tuple[int, int, int]] = []
    for e in entries:
        if e not in seen:
            seen.add(e)
            unique.append(e)
    # Keep frequency ordering deterministic: re-sort by frequency in the
    # source with RGB tiebreak, restricted to the quantized set.
    allowed = set(unique)
    counts = Counter(c for c in opaque if c in allowed)
    # Colors in the quantized palette that never occur map from nearest
    # source colors; keep them at the end in palette order.
    ranked = sorted(
        [c for c in unique if c in counts],
        key=lambda c: (-counts[c], c),
    )
    tail = [c for c in unique if c not in counts]
    return ranked + tail


def quantize_face_lut4(
    rgba: list[tuple[int, int, int, int]], width: int, height: int,
) -> tuple[list[int], bytes]:
    """One baked face as a LUT4 texture: (16 RGB555 entries, packed texels).

    Each face gets its own palette, fitted to the few colors a small patch of
    the source texture actually holds, instead of sharing one 256-color bank
    across the whole model. Texel code 0 is left unused: with the sprite's
    transparency code active it would punch a hole, so a face keeps 15
    colors. Colors are reduced to RGB555 first -- the hardware cannot show
    the difference, and it lets a face with few distinct colors keep them
    exactly. Entries are sorted so faces with the same colors share a table.
    Texels pack two per byte, leftmost in the high nibble.
    """
    from PIL import Image

    rgb555 = [rgb888_to_rgb555(r, g, b) for (r, g, b, _a) in rgba]
    distinct = sorted(set(rgb555))
    if len(distinct) <= 15:
        colors = distinct
        index_of = {c: i for i, c in enumerate(colors)}
        codes = [index_of[c] for c in rgb555]
    else:
        img = Image.new("RGB", (width, height))
        img.putdata([(r, g, b) for (r, g, b, _a) in rgba])
        q = img.quantize(colors=15, method=Image.Quantize.MEDIANCUT, kmeans=2,
                         dither=Image.Dither.NONE)
        pal = q.getpalette() or []
        indices = list(q.tobytes())  # mode "P": one palette index per byte
        raw = {i: rgb888_to_rgb555(pal[i * 3], pal[i * 3 + 1], pal[i * 3 + 2])
               for i in set(indices)}
        colors = sorted(set(raw.values()))
        index_of = {c: i for i, c in enumerate(colors)}
        codes = [index_of[raw[i]] for i in indices]
    lut = [colors[0]] + colors + [colors[-1]] * (15 - len(colors))
    packed = bytearray()
    for y in range(height):
        row = codes[y * width:(y + 1) * width]
        for x in range(0, width, 2):
            packed.append(((row[x] + 1) << 4) | (row[x + 1] + 1))
    return lut, bytes(packed)


def _lut_code_palette(baked_rgba, lo: int, hi: int):
    """One shared palette in codes lo..hi, and a LUT4 entry -> code mapper.

    An 8-bit/pixel VDP1 framebuffer (the 640/704-wide hi-res modes) keeps
    only the low byte of what a lookup table yields, which VDP2 then reads as
    a colour-RAM index: RGB table entries are not allowed there. The whole
    model therefore shares one palette of hi - lo + 1 colours, and each face's
    15-colour table holds the codes of its nearest palette colours. Codes
    outside lo..hi stay free for the transparent code 0, the sprite shadow
    code and whatever the program draws next to the model.
    Returns (256-entry RGB555 palette, mapper from an RGB555 LUT entry).
    """
    opaque = [(r, g, b) for face in baked_rgba for (r, g, b, _a) in face]
    count = hi - lo + 1
    distinct = sorted(set(opaque))
    entries = distinct if len(distinct) <= count else _quantize_colors(opaque, count)
    entries = entries[:count]
    palette = [0x0000] * 256
    for i, (r, g, b) in enumerate(entries):
        palette[lo + i] = rgb888_to_rgb555(r, g, b)
    cache: dict[int, int] = {}

    def snap(c555: int) -> int:
        hit = cache.get(c555)
        if hit is None:
            r = (c555 & 0x1F) << 3
            g = ((c555 >> 5) & 0x1F) << 3
            b = ((c555 >> 10) & 0x1F) << 3
            best = min(range(len(entries)), key=lambda i: (
                (entries[i][0] - r) ** 2 + (entries[i][1] - g) ** 2
                + (entries[i][2] - b) ** 2))
            hit = cache[c555] = lo + best
        return hit

    return palette, snap


def map_faces_to_indices(
    baked_rgba: list[list[tuple[int, int, int, int]]],
    face_sizes: list[tuple[int, int]],
    palette_rgb888: list[tuple[int, int, int]],
) -> list[bytes]:
    from PIL import Image

    pal_img = Image.new("P", (16, 16))
    flat: list[int] = []
    for (r, g, b) in palette_rgb888:
        flat.extend([r, g, b])
    while len(flat) < 256 * 3:
        flat.extend([0, 0, 0])
    pal_img.putpalette(flat[: 256 * 3])

    out: list[bytes] = []
    for face, (w, h) in zip(baked_rgba, face_sizes):
        # Transparent texels must map to index 0: Pillow's quantize has no
        # alpha concept, so remap in two steps. Build an RGB image where
        # transparent texels are painted with the reserved palette entry 0
        # color only if that color is unique to transparency; otherwise paint
        # them magenta and fix up afterwards. Simpler and exact: map opaque
        # texels through Pillow, force transparent texels to 0.
        rgb_pixels = [(r, g, b) for (r, g, b, a) in face]
        img = Image.new("RGB", (w, h))
        img.putdata(rgb_pixels)
        q = img.quantize(palette=pal_img, dither=Image.Dither.NONE)
        get_flat = getattr(q, "get_flattened_data", None)
        indices = list(get_flat()) if callable(get_flat) else list(q.getdata())
        fixed = bytearray(len(indices))
        for i, ((r, g, b, a), idx) in enumerate(zip(face, indices)):
            if a < 128:
                fixed[i] = 0
            else:
                fixed[i] = idx
                # Never let an opaque texel claim the transparent index when
                # transparency is in use: if Pillow mapped an opaque color to
                # 0 (because palette[0] is black and the texel is black),
                # redirect it to the nearest non-zero entry.
                if palette_rgb888 and len(palette_rgb888) > 1 and idx == 0 and (0, 0, 0) in palette_rgb888:
                    # palette[0] is the reserved transparent slot; find best
                    # non-zero match for this opaque color.
                    best = 1
                    best_d = None
                    for pi in range(1, len(palette_rgb888)):
                        pr, pg, pb = palette_rgb888[pi]
                        d = (pr - r) ** 2 + (pg - g) ** 2 + (pb - b) ** 2
                        if best_d is None or d < best_d:
                            best_d = d
                            best = pi
                    fixed[i] = best
        out.append(bytes(fixed))
    return out


# ----------------------------------------------------------------------
# Import driver
# ----------------------------------------------------------------------

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


# ----------------------------------------------------------------------
# Animated glTF/GLB import (Saturn baked-vertex path)
# ----------------------------------------------------------------------
#
# Pipeline (host-side; the Saturn sees only the generated C/H):
#
#   parse source -> select clips -> sample times -> bake poses (all clips)
#        |
#        v
#   animation/silhouette importance -> quality-gated simplification
#        |
#        +------> bake per-clip pose frames on the shared topology
#        +------> bake face textures on the simplified topology
#                        |
#                        v
#                   dedup + palette -> generated C/H + JSON report
#
# Source skeletal data is baked to vertex animation: no joints, weights,
# inverse bind matrices, source UVs or GLB structures reach the runtime.

@dataclass
class AnimatedImportResult:
    static: ImportResult
    animations: list[dict]
    report: dict


def select_animation_clips(model, selector: str) -> list[int]:
    if selector == "all":
        if not model.clips:
            raise ImportError("model has no animation clips")
        return list(range(len(model.clips)))
    if "," in selector:
        # A comma list keeps only the clips a program plays, in that order,
        # so a many-clip rig fits the pose-stream budget.
        picked: list[int] = []
        for part in selector.split(","):
            for index in select_animation_clips(model, part.strip()):
                if index not in picked:
                    picked.append(index)
        return picked
    try:
        index = int(selector)
        if index < 0 or index >= len(model.clips):
            raise ImportError(
                f"--animation {selector}: only {len(model.clips)} clip(s) available"
            )
        return [index]
    except ValueError:
        for i, clip in enumerate(model.clips):
            if clip.name == selector:
                return [i]
        raise ImportError(
            f"--animation {selector!r}: no such clip "
            f"(have: {[c.name for c in model.clips]})"
        )


def animated_frame_times(model, clip, fps: str) -> tuple[list[float], bool]:
    """Runtime sample times for one clip plus loop-duplicate flag."""
    if fps == "source":
        return anim_eval.runtime_frame_times(model, clip)
    try:
        rate = float(fps)
    except ValueError:
        raise ImportError(f"--animation-fps must be 'source' or a number (got {fps!r})")
    if rate <= 0.0:
        raise ImportError(f"--animation-fps must be positive (got {fps!r})")
    n = int(clip.duration * rate)
    times = [min(i / rate, clip.duration) for i in range(n + 1)]
    if times[-1] < clip.duration:
        times.append(clip.duration)
    seen: dict[float, None] = {}
    for t in times:
        seen[float(t)] = None
    ordered = sorted(seen)
    # A terminal sample equal to the first pose is a redundant loop frame.
    poses = anim_eval.bake_clip_poses(model, clip, [ordered[0], ordered[-1]])
    removed = False
    if len(ordered) > 1 and anim_eval.loop_pose_distance(poses[0], poses[1]) <= 1e-5:
        ordered = ordered[:-1]
        removed = True
    return ordered, removed


def rate_fraction(frame_count: int, duration: float) -> tuple[int, int]:
    """Reduced (num, den) frames-per-second fraction for the runtime."""
    if duration <= 0.0 or frame_count <= 0:
        raise ImportError("cannot derive a sample rate from an empty clip")
    num = int(round(frame_count / duration * 1000000))
    den = 1000000
    g = math.gcd(num, den)
    return num // g, den // g


def _face_command_cap(args, profile) -> int | None:
    cap: int | None = None
    if args.target == "saturn" or args.profile is not None:
        cap = saturn_profile_mod.face_command_budget(profile)
    if args.max_vdp1_commands is not None:
        room = (args.max_vdp1_commands - profile.setup_commands - profile.end_commands
                - profile.hud_reserve - profile.min_command_headroom)
        if room < 1:
            raise ImportError("--max-vdp1-commands leaves no room for model faces")
        cap = room if cap is None else min(cap, room)
    if args.max_triangles is not None:
        cap = args.max_triangles if cap is None else min(cap, args.max_triangles)
    return cap


def _glb_winding(tris, uvs, reverse_winding):
    """glTF CCW triangles to LibSaturn A/B/C/D degenerate quads.

    Default reverses to clockwise (outward under cross(D-A, B-A)), the same
    convention as the OBJ path; --reverse-winding keeps source order.
    Returns (quads, quad_uvs) with duplicated corners carrying duplicated
    UVs so the canonical rectangle collapses with the geometric quad.
    """
    quads, quad_uvs = [], []
    for (a, b, c) in tris:
        ua, ub, uc = uvs[a], uvs[b], uvs[c]
        if reverse_winding:
            quads.append((a, b, c, c))
            quad_uvs.append((ua, ub, uc, uc))
        else:
            quads.append((a, c, b, b))
            quad_uvs.append((ua, uc, ub, ub))
    return quads, quad_uvs


def _polygon_winding(polys, uvs, reverse_winding):
    """Source CCW triangles and merged quads to LibSaturn A/B/C/D faces.

    Same convention as _glb_winding: the default reverses to clockwise,
    and a triangle repeats its last corner so its baked rectangle collapses
    with the geometric quad.
    """
    quads, quad_uvs = [], []
    for poly in polys:
        order = list(poly) if reverse_winding else [poly[0]] + list(reversed(poly[1:]))
        if len(order) == 3:
            order.append(order[2])
        quads.append(tuple(order))
        quad_uvs.append(tuple(uvs[i] for i in order))
    return quads, quad_uvs


def _cheapest_rotation(quad, quad_uv, img_w, img_h, texture_scale, max_w, max_h):
    """Cyclic corner rotation whose baked rectangle needs the fewest texels.

    Every rotation draws the same outline and keeps the winding; it only
    decides which edge runs along the texture rows. Ties keep the original
    order so output stays deterministic.
    """
    best = None
    for r in range(4):
        q = quad[r:] + quad[:r]
        uv = quad_uv[r:] + quad_uv[:r]
        w, h = estimate_face_size(uv, img_w, img_h, texture_scale)
        w = max(8, ((w + 7) // 8) * 8)
        if w > max_w or h > max_h:
            continue
        if best is None or w * h < best[0]:
            best = (w * h, q, uv)
    if best is None:
        return quad, quad_uv
    return best[1], best[2]


def _resolve_material_weights(model, specs) -> dict[int, float]:
    """``NAME=W`` / ``INDEX=W`` strings to {material index: weight}."""
    weights: dict[int, float] = {}
    names = [m.get("name") for m in model.materials]
    for spec in specs or []:
        key, sep, value = spec.rpartition("=")
        if not sep or not key:
            raise ImportError(f"--material-weight expects NAME=WEIGHT (got {spec!r})")
        try:
            weight = float(value)
        except ValueError:
            raise ImportError(f"--material-weight {spec!r}: weight is not a number")
        if weight <= 0.0:
            raise ImportError(f"--material-weight {spec!r}: weight must be positive")
        if key in names:
            index = names.index(key)
        elif key.isdigit() and int(key) < len(names):
            index = int(key)
        else:
            raise ImportError(f"--material-weight {spec!r}: no material {key!r} "
                              f"(have: {', '.join(str(n) for n in names)})")
        weights[index] = weight
    return weights


def _locality_order(quads, face_texture_indices, vertices_fx, animations, positions):
    """Reorder faces along the model's longest axis and vertices by first use.

    Any contiguous run of faces then touches a mostly contiguous run of
    vertices, so a caller that splits the face list between CPUs can hand
    each one a vertex window instead of the whole pose to project. Returns
    the reordered (quads, face_texture_indices, vertices_fx) and rewrites the
    animation pose streams in place.
    """
    lo = [min(p[a] for p in positions) for a in range(3)]
    hi = [max(p[a] for p in positions) for a in range(3)]
    axis = max(range(3), key=lambda a: hi[a] - lo[a])
    key = [sum(positions[i][axis] for i in q) / 4.0 for q in quads]
    face_order = sorted(range(len(quads)), key=lambda f: (key[f], f))
    new_of: dict[int, int] = {}
    for f in face_order:
        for i in quads[f]:
            if i not in new_of:
                new_of[i] = len(new_of)
    for i in range(len(vertices_fx)):  # unreferenced vertices keep a slot
        if i not in new_of:
            new_of[i] = len(new_of)
    old_at = [0] * len(new_of)
    for old, new in new_of.items():
        old_at[new] = old
    for anim in animations:
        nv = anim["vertex_count"]
        stream = anim["stream"]
        out: list[int] = []
        for frame in range(anim["frame_count"]):
            base = frame * nv * 3
            for old in old_at:
                out.extend(stream[base + old * 3: base + old * 3 + 3])
        anim["stream"] = out
    return ([tuple(new_of[i] for i in quads[f]) for f in face_order],
            [face_texture_indices[f] for f in face_order],
            [vertices_fx[old] for old in old_at])


def import_animated_model(
    glb_path: Path,
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
    simplify: str = "auto",
    quality: str = "balanced",
    max_triangles: int | None = None,
    max_vdp1_commands: int | None = None,
    max_pose_stream_bytes: int = 256 * 1024,
    hud_reserve: int = 128,
    animation: str = "all",
    animation_fps: str = "source",
    merge_rigid_meshes: bool = False,
    generate_lods: bool = False,
    silhouette_views: int = 16,
    animation_weight: float = 1.0,
    silhouette_weight: float = 1.0,
    face_colors: str = "off",
    light_dir=(-0.5, 0.6, 0.8),
    ambient: float = 0.35,
    diffuse: float = 0.75,
    merge_quads: bool = False,
    quad_max_texel_error: float = 1.0,
    quad_max_fold_deg: float = 30.0,
    material_weights: list[str] | None = None,
    weld_vertices: bool = False,
    texture_format: str = "indexed8",
    locality_order: bool = False,
    texel_extent: float | None = None,
    lut_codes: tuple[int, int] | None = None,
) -> AnimatedImportResult:
    if palette_index < 0 or palette_index > 7:
        raise ImportError(f"--palette-index must be in 0..7 (got {palette_index})")
    if face_colors not in ("off", "auto", "on"):
        raise ImportError(f"--face-colors must be off|auto|on (got {face_colors!r})")
    if ambient < 0.0 or diffuse < 0.0:
        raise ImportError("--ambient and --diffuse must not be negative")
    try:
        light = face_color_mod.parse_light_dir(light_dir)
    except gltf_mod.GltfError as exc:
        raise ImportError(str(exc))
    if texture_scale <= 0.0:
        raise ImportError(f"--texture-scale must be positive (got {texture_scale})")
    if scale <= 0.0:
        raise ImportError(f"--scale must be positive (got {scale})")
    if lut_codes is not None:
        lo, hi = lut_codes
        if texture_format != "lut4":
            raise ImportError("--lut-codes needs --texture-format lut4")
        if not 1 <= lo <= hi <= 255:
            raise ImportError(f"--lut-codes must be LO-HI within 1..255 (got {lo}-{hi})")
    if texel_extent is not None and texel_extent <= 0.0:
        raise ImportError(f"--texel-extent must be positive (got {texel_extent})")
    if texture_format not in ("indexed8", "lut4"):
        raise ImportError(f"--texture-format must be indexed8|lut4 (got {texture_format!r})")
    if locality_order and face_colors != "off":
        raise ImportError("--locality-order is for textured faces; solid-color "
                          "shade streams are baked in source face order")
    if weld_vertices and face_colors != "off":
        raise ImportError("--weld-vertices is for textured faces; --face-colors "
                          "already welds and bakes per-vertex light")
    if merge_quads and face_colors != "off":
        raise ImportError("--merge-quads needs textured faces (--face-colors off): "
                          "solid-color shades are baked per triangle")
    if quality not in metrics_mod.QUALITY_PRESETS:
        raise ImportError(f"unknown --quality {quality!r}")

    try:
        glb = gltf_mod.parse_model(glb_path)
        model = srcmodel.from_gltf(
            glb, glb_path.stem, merge_rigid_meshes=merge_rigid_meshes
        )
    except gltf_mod.GltfError as exc:
        raise ImportError(str(exc))
    stats = srcmodel.source_stats(model)
    material_weight_map = _resolve_material_weights(model, material_weights)
    # Solid-color mode replaces the model BEFORE importance, poses and
    # simplification: every later stage then works on the welded vertices.
    color_report: dict = {"mode": face_colors, "enabled": False}
    face_levels = 0
    base_colors: list = []
    if face_colors != "off":
        analysis = face_color_mod.analyze(model)
        enable, reason = face_color_mod.decide(face_colors, analysis)
        color_report.update({
            "uniform_faces": analysis.uniform,
            "non_uniform_faces": analysis.non_uniform,
            "distinct_colors": analysis.distinct_colors,
            "reason": reason,
        })
        if face_colors == "on" and not enable:
            raise ImportError(f"--face-colors on: {reason}")
        if enable:
            source_vertices = len(model.vertices)
            model, base_colors, welded_count = face_color_mod.flatten(model, analysis)
            face_levels = face_color_mod.levels_for(len(base_colors))
            color_report.update({
                "enabled": True,
                "levels": face_levels,
                "palette_entries": 1 + len(base_colors) * face_levels,
                "light_dir": list(light),
                "ambient": ambient,
                "diffuse": diffuse,
                "welded_vertices": welded_count,
                "vertices_before_weld": source_vertices,
                "vertices_after_weld": len(model.vertices),
            })

    if not model.textures and not color_report["enabled"]:
        raise ImportError(
            f"{glb_path}: no embedded textures found and solid face colors are disabled"
        )

    clip_ids = select_animation_clips(model, animation)
    per_clip_times: dict[int, list[float]] = {}
    loop_flags: dict[int, bool] = {}
    for ci in clip_ids:
        times, removed = animated_frame_times(model, model.clips[ci], animation_fps)
        if not times:
            raise ImportError(f"clip '{model.clips[ci].name}' produced no sample times")
        per_clip_times[ci] = times
        loop_flags[ci] = removed

    # Bake every clip's poses on the SOURCE topology for importance and
    # worst-pose collapse costs; the shared simplified topology is baked
    # again afterwards (subset positions make that exact).
    all_poses: list = []
    for ci in clip_ids:
        all_poses.extend(anim_eval.bake_clip_poses(model, model.clips[ci], per_clip_times[ci]))
    metric_times = per_clip_times[clip_ids[0]]
    first_clip = model.clips[clip_ids[0]]
    anim_imp = metrics_mod.compute_animation_importance(model, first_clip, metric_times)
    if len(clip_ids) > 1:
        for ci in clip_ids[1:]:
            extra = metrics_mod.compute_animation_importance(model, model.clips[ci], per_clip_times[ci])
            anim_imp = [max(a, b) for a, b in zip(anim_imp, extra)]
    sil_imp = sil_mod.compute_silhouette_importance(
        model, clip=first_clip,
        times=metric_times[:: max(1, len(metric_times) // 8)][:8],
        n_views=silhouette_views,
    )

    profile = saturn_profile_mod.SaturnProfile()
    if max_pose_stream_bytes < 1:
        raise ImportError("--max-pose-stream-bytes must be positive")
    profile.max_pose_stream_bytes = max_pose_stream_bytes
    if hud_reserve < 0:
        raise ImportError("--hud-reserve must not be negative")
    profile.hud_reserve = hud_reserve
    face_cap = _face_command_cap(
        argparse.Namespace(target="saturn", profile=None, max_triangles=max_triangles,
                           max_vdp1_commands=max_vdp1_commands),
        profile,
    )
    if simplify == "off":
        simp = simp_mod.simplify(
            model, anim_importance=anim_imp, sil_importance=sil_imp,
            options=simp_mod.SimplificationOptions(
                target_triangles=len(model.triangles), quality=quality,
                animation_weight=animation_weight, silhouette_weight=silhouette_weight,
                material_weights=material_weight_map),
            pose_positions=all_poses,
        )
        quality_report = metrics_mod.evaluate_candidate(
            model, simp, first_clip, quality, metric_times, silhouette_views)
        if not quality_report["passed"]:
            raise ImportError(
                f"full-resolution model fails quality preset {quality!r}: "
                f"{quality_report['failing_gates']}"
            )
    else:
        if simplify == "auto":
            requested = face_cap if face_cap is not None else len(model.triangles)
        else:
            try:
                requested = int(simplify)
            except ValueError:
                raise ImportError(f"--simplify must be off|auto|TARGET (got {simplify!r})")
            if requested < 1:
                raise ImportError(f"--simplify target must be >= 1 (got {simplify!r})")
        try:
            simp, quality_report = metrics_mod.search_upward(
                model, first_clip, requested, quality,
                simp_mod.SimplificationOptions(
                    target_triangles=requested, quality=quality,
                    animation_weight=animation_weight, silhouette_weight=silhouette_weight,
                    material_weights=material_weight_map),
                anim_importance=anim_imp, sil_importance=sil_imp,
                times=metric_times, pose_positions=all_poses,
                # An explicit numeric --simplify target is a floor: honor the
                # requested density when it passes instead of minimizing away.
                enforce_floor=(simplify != "auto"),
            )
        except gltf_mod.GltfError as exc:
            raise ImportError(str(exc))
    delivered = len(simp.triangles)
    # Merged quads are checked against the command cap once they exist: a
    # pair costs one command, so the triangle count may exceed the cap.
    if face_cap is not None and delivered > face_cap and not merge_quads:
        raise ImportError(saturn_profile_mod.format_hard_failure(
            delivered, face_cap,
            f"smallest {quality}-valid mesh has {delivered} triangles"))

    # Mirror-flip handling matches the OBJ path: odd-axis mirrors toggle
    # the winding reversal so outward normals stay outward.
    if (int(bool(flip_x)) + int(bool(flip_y)) + int(bool(flip_z))) % 2 == 1:
        reverse_winding = not reverse_winding

    def _flip(p):
        x, y, z = p
        return (-x if flip_x else x, -y if flip_y else y, -z if flip_z else z)

    # Bake per-clip pose frames on the shared simplified topology.
    simp_view = metrics_mod.simplified_as_source(simp, model)
    animations: list[dict] = []
    all_frames: list = []
    clip_frames: dict[int, list] = {}
    for ci in clip_ids:
        frames = anim_eval.bake_clip_poses(simp_view, model.clips[ci], per_clip_times[ci])
        clip_frames[ci] = [[_flip(p) for p in frame] for frame in frames]
        all_frames.extend(clip_frames[ci])
    # Runtime vertices: every simplified vertex, or one per point that moves
    # identically in every frame. Faces bake their textures from source UVs
    # at import time, so the Saturn never needs a UV-split copy.
    runtime_of = list(range(len(simp.positions)))
    runtime_src = list(range(len(simp.positions)))
    if weld_vertices:
        runtime_src = []
        seen: dict[tuple, int] = {}
        for vi, p in enumerate(simp.positions):
            key = (tuple(round(c, 7) for c in p),) + tuple(
                tuple(round(c, 7) for c in frame[vi]) for frame in all_frames)
            if key not in seen:
                seen[key] = len(runtime_src)
                runtime_src.append(vi)
            runtime_of[vi] = seen[key]
    weld_report = {"enabled": bool(weld_vertices),
                   "vertices_before": len(simp.positions),
                   "vertices_after": len(runtime_src)}
    for ci in clip_ids:
        clip = model.clips[ci]
        times = per_clip_times[ci]
        frames = clip_frames[ci]
        baked = pose_bake.quantize_frames(
            [[frame[vi] for vi in runtime_src] for frame in frames],
            scale=scale, bbox_diagonal=simp_view.bbox_diagonal() * scale)
        if face_levels:
            baked["shades"] = face_color_mod.bake_shades(
                frames, simp.triangles, simp.tri_materials, face_levels, light,
                clockwise_front=reverse_winding)
            baked["vertex_gouraud"] = face_color_mod.bake_vertex_gouraud(
                frames, simp.triangles, len(simp.positions), light, ambient, diffuse,
                clockwise_front=reverse_winding)
        num, den = rate_fraction(len(times), clip.duration or 1.0) \
            if animation_fps == "source" else (int(float(animation_fps)), 1)
        baked["name"] = clip.name
        baked["sample_rate_num"] = num
        baked["sample_rate_den"] = den
        baked["loop"] = loop_flags[ci]
        baked["frame_times"] = list(times)
        animations.append(baked)

    # Static geometry: bind pose with flips/scale, LibSaturn winding.
    bind = [_flip(p) for p in simp.positions]
    bind = [(x * scale, y * scale, z * scale) for (x, y, z) in bind]
    quad_report = {"enabled": False}
    if merge_quads:
        texel_scales = {}
        for mt, mat in enumerate(model.materials):
            if "texture" in mat:
                tex = model.textures[mat["texture"]]
                texel_scales[mt] = (tex.width * texture_scale, tex.height * texture_scale)
        polys, poly_materials, quad_report = quad_merge_mod.merge_quads(
            simp.triangles, simp.tri_materials, simp.uvs,
            [_flip(p) for p in simp.positions], all_frames, texel_scales,
            quad_merge_mod.QuadMergeOptions(
                max_texel_error=quad_max_texel_error, max_fold_deg=quad_max_fold_deg))
        quads, quad_uvs = _polygon_winding(polys, simp.uvs, reverse_winding)
        face_materials = poly_materials
        delivered = len(quads)
        if face_cap is not None and delivered > face_cap:
            raise ImportError(saturn_profile_mod.format_hard_failure(
                delivered, face_cap,
                f"{len(simp.triangles)} triangles merge to {delivered} faces"))
    else:
        quads, quad_uvs = _glb_winding(simp.triangles, simp.uvs, reverse_winding)
        face_materials = simp.tri_materials
    # Compact bind arrays in simplified order (simp.positions already is).
    vertices_fx = [(float_to_fx16(x), float_to_fx16(y), float_to_fx16(z))
                   for (x, y, z) in (bind[vi] for vi in runtime_src)]
    for i, (x, y, z) in enumerate(vertices_fx):
        if not -(2**31) <= x < 2**31 or not -(2**31) <= y < 2**31 or not -(2**31) <= z < 2**31:
            raise ImportError(f"vertex {i} overflows 16.16 fixed point (reduce --scale)")

    # Canonical face-texture baking from the SIMPLIFIED topology (textured
    # assets only; a solid-color asset's faces are palette shades).
    baked_rgba: list = []
    face_sizes: list[tuple[int, int]] = []
    face_mtls: list[str] = []
    texel_density = None
    if texel_extent is not None:
        # Texels per world unit such that the bind pose's longest extent spans
        # texel_extent texels: a face never bakes more texels than it covers
        # on screen when the model spans that many pixels.
        span = max(max(p[i] for p in bind) - min(p[i] for p in bind) for i in range(3))
        texel_density = texel_extent / max(span, 1e-9)
    for qi, ((a, b, c, d), (ua, ub, uc, ud)) in enumerate(zip(quads, quad_uvs)):
        if face_levels:
            break
        mt = face_materials[qi]
        mat = model.materials[mt] if mt < len(model.materials) else {}
        if "texture" not in mat:
            raise ImportError(
                f"face {qi} uses material {mat.get('name', mt)!r} without a "
                "baseColorTexture (the animated textured path needs a texture "
                "on every material)"
            )
        tex = model.textures[mat["texture"]]
        if merge_quads:
            (a, b, c, d), (ua, ub, uc, ud) = _cheapest_rotation(
                (a, b, c, d), (ua, ub, uc, ud), tex.width, tex.height, texture_scale,
                max_texture_width, max_texture_height)
            quads[qi] = (a, b, c, d)
        est_w, est_h = estimate_face_size((ua, ub, uc, ud), tex.width, tex.height, texture_scale)
        if texel_density is not None:
            pa, pb, pc, pd = (bind[i] for i in (a, b, c, d))
            world_w = max(math.dist(pa, pb), math.dist(pd, pc)) * texel_density
            world_h = max(math.dist(pa, pd), math.dist(pb, pc)) * texel_density
            est_w = max(1, min(est_w, math.ceil(world_w)))
            est_h = max(1, min(est_h, math.ceil(world_h)))
        out_w, out_h = conform_size(est_w, est_h, max_texture_width, max_texture_height,
                                    mat.get("name"), qi)
        baked_rgba.append(bake_face_rgba((ua, ub, uc, ud), tex.width, tex.height,
                                         tex.pixels_rgba, out_w, out_h, sampling))
        face_sizes.append((out_w, out_h))
        face_mtls.append(mat.get("name", str(mt)))

    shade_palette = None
    face_base = None
    has_transparency = False
    luts: list[tuple[int, ...]] = []
    face_luts: list[int] = []
    if face_levels:
        palette_rgb555: list[int] = []
        indexed_faces: list = []
        shade_palette = face_color_mod.shade_palette(base_colors, face_levels, ambient, diffuse)
        face_base = face_color_mod.face_base_shades(simp.tri_materials, face_levels)
    elif texture_format == "lut4":
        if any(a < 128 for face in baked_rgba for (_, _, _, a) in face):
            raise ImportError("--texture-format lut4 needs opaque textures")
        palette_rgb555 = []
        indexed_faces = []
        lut_of: dict[tuple[int, ...], int] = {}
        snap = None
        if lut_codes is not None:
            palette_rgb555, snap = _lut_code_palette(baked_rgba, *lut_codes)
        for rgba, (w, h) in zip(baked_rgba, face_sizes):
            lut, packed = quantize_face_lut4(rgba, w, h)
            if snap is not None:
                lut = [snap(c) for c in lut]
            key = tuple(lut)
            if key not in lut_of:
                lut_of[key] = len(luts)
                luts.append(key)
            face_luts.append(lut_of[key])
            indexed_faces.append(packed)
    else:
        palette_rgb888, has_transparency, _ = build_shared_palette(baked_rgba)
        indexed_faces = map_faces_to_indices(baked_rgba, face_sizes, palette_rgb888)
        palette_rgb555 = []
        for i, (r, g, b) in enumerate(palette_rgb888):
            palette_rgb555.append(0x0000 if (has_transparency and i == 0) else rgb888_to_rgb555(r, g, b))
        while len(palette_rgb555) < 256:
            palette_rgb555.append(0x0000)
        palette_rgb555 = palette_rgb555[:256]
    opaque_flag = 0x0001 if not has_transparency else 0x0000
    if luts:
        opaque_flag |= 0x8000  # SAT_MODEL_TEXTURE_LUT4
    unique: list[dict] = []
    key_to_index: dict[tuple, int] = {}
    face_texture_indices: list[int] = [0xFFFF] * len(quads) if face_levels else []
    for fi, ((w, h), pixels) in enumerate(zip(face_sizes, indexed_faces)):
        slot = face_luts[fi] if luts else 0
        key = (w, h, bytes(pixels), slot, opaque_flag)
        if key in key_to_index:
            face_texture_indices.append(key_to_index[key])
        else:
            idx = len(unique)
            key_to_index[key] = idx
            unique.append({"width": w, "height": h, "pixels": bytes(pixels),
                           "flags": opaque_flag, "pixel_count": w * h,
                           "palette_slot": slot})
            face_texture_indices.append(idx)
    luts_rgb555 = [c for lut in luts for c in lut]

    # Faces baked from simplified-vertex UVs; now point them at runtime ones.
    quads = [tuple(runtime_of[i] for i in q) for q in quads]
    if locality_order:
        quads, face_texture_indices, vertices_fx = _locality_order(
            quads, face_texture_indices, vertices_fx, animations,
            [all_frames[0][vi] for vi in runtime_src])
    static = ImportResult(
        vertices_fx=vertices_fx,
        indices_abcd=quads,
        face_texture_indices=face_texture_indices,
        textures=unique,
        palette_rgb555=palette_rgb555,
        palette_base=palette_index,
        stats={},
        shade_palette_rgb555=shade_palette,
        face_base_shades=face_base,
        luts_rgb555=luts_rgb555,
    )
    # VRAM per texture is its stored bytes; each LUT is another 32.
    texture_sizes = [len(t["pixels"]) for t in unique] + [32] * len(luts)
    indexed_bytes = sum(texture_sizes)
    vram_est = sum(((n + 7) & ~7) for n in texture_sizes)
    largest = max((t["width"] * t["height"], t["width"], t["height"]) for t in unique) if unique else (0, 0, 0)
    shade_bytes = sum(len(a.get("shades") or []) for a in animations)
    gouraud_bytes = sum(len(a.get("vertex_gouraud") or []) for a in animations)
    if face_levels:
        color_report["shade_bytes"] = shade_bytes
        color_report["gouraud_bytes"] = gouraud_bytes + len(face_base or [])
    # Shade and Gouraud streams are per-frame data like poses and share
    # their budget.
    pose_bytes = sum(a["pose_bytes"] for a in animations) + shade_bytes + gouraud_bytes
    resource_report = saturn_profile_mod.check_resources(
        profile, faces=delivered, texture_payload_bytes=indexed_bytes,
        texture_sizes=texture_sizes,
        pose_stream_bytes=pose_bytes)
    if not resource_report["passed"]:
        raise ImportError(saturn_profile_mod.format_hard_failure(
            delivered, face_cap or saturn_profile_mod.face_command_budget(profile),
            "; ".join(resource_report["failing_gates"])))

    lod_reports = None
    if generate_lods:
        budget = face_cap or saturn_profile_mod.face_command_budget(profile)
        specs = lod_mod.default_lod_specs(budget, len(model.triangles))
        lod_reports = []
        for entry in lod_mod.generate_lods(model, first_clip, specs, quality,
                                           anim_imp, sil_imp, metric_times, all_poses,
                                           silhouette_views):
            lod_reports.append({k: v for k, v in entry.items()
                                if k not in ("simplified", "quality")})
            lod_reports[-1]["passed"] = entry["passed"]
            lod_reports[-1]["failing_gates"] = entry["failing_gates"]

    static.stats = {
        "source_vertices": stats["vertices"],
        "source_triangles": stats["triangles"],
        "source_materials": stats["materials"],
        "source_textures": stats["textures"],
        "source_joints": stats["joints"],
        "animation_clips": stats["animation_clips"],
        "selected_clips": [model.clips[ci].name for ci in clip_ids],
        "baked_faces_before_dedup": len(baked_rgba),
        "unique_textures_after_dedup": len(unique),
        "palette_count": 1,
        "indexed_texture_bytes": indexed_bytes,
        "palette_bytes": 512,
        "estimated_vram_usage": vram_est,
        "largest_baked_texture": (largest[1], largest[2]) if unique else (0, 0),
        "has_transparency": has_transparency,
        "scale": scale,
    }
    report = {
        "source": {
            **stats,
            "clip_durations": {model.clips[ci].name: model.clips[ci].duration for ci in clip_ids},
        },
        "simplification": {**simp.report, "quality_preset": quality},
        "quad_merge": quad_report,
        "vertex_weld": weld_report,
        "animation_quality": quality_report,
        "saturn_animation": [
            {
                "clip": a["name"],
                "baked_frames": a["frame_count"],
                "sample_rate_num": a["sample_rate_num"],
                "sample_rate_den": a["sample_rate_den"],
                "loop": a["loop"],
                "duplicate_loop_frame_removed": loop_flags[ci],
                "position_encoding": "int16 scale/bias per axis",
                "pose_stream_bytes": a["pose_bytes"],
                "quantization_max_error": a["max_error"],
                "quantization_mean_error": a["mean_error"],
            }
            for a, ci in zip(animations, clip_ids)
        ],
        "textures": {
            "baked_faces": len(baked_rgba),
            "unique_textures": len(unique),
            "format": texture_format if not face_levels else "none",
            "luts": len(luts),
            "indexed_pixel_bytes": indexed_bytes,
            "palette_bytes": 512,
            "estimated_vram_bytes": vram_est,
        },
        "face_colors": color_report,
        "vdp1": resource_report,
        "lods": lod_reports,
        "result": {"pass": bool(quality_report["passed"] and resource_report["passed"])},
    }
    return AnimatedImportResult(static=static, animations=animations, report=report)


def print_animated_stats(report: dict) -> None:
    src = report["source"]
    simp = report["simplification"]
    print(f"source vertices: {src['vertices']}")
    print(f"source triangles: {src['triangles']}")
    print(f"source joints: {src['joints']}")
    print(f"animation clips: {src['animation_clips']}")
    print(f"requested target: {simp['requested_target']}")
    print(f"delivered triangles: {simp['delivered_triangles']}")
    print(f"delivered vertices: {simp['delivered_vertices']}")
    print(f"reduction: {simp['reduction_percent']}%")
    aq = report["animation_quality"]["surface"]
    print(f"animated surface error: max {aq['max']:.4f} p95 {aq['p95']:.4f} mean {aq['mean']:.4f}")
    for anim in report["saturn_animation"]:
        print(f"clip '{anim['clip']}': {anim['baked_frames']} frames @ "
              f"{anim['sample_rate_num']}/{anim['sample_rate_den']} Hz, loop={anim['loop']}, "
              f"pose bytes {anim['pose_stream_bytes']}, quant err {anim['quantization_max_error']:.6f}")
    colors = report.get("face_colors", {})
    if colors.get("enabled"):
        print(f"face colors: {colors['distinct_colors']} colors x {colors['levels']} light levels "
              f"({colors['non_uniform_faces']} non-uniform faces averaged), "
              f"vertices welded {colors['vertices_before_weld']} -> {colors['vertices_after_weld']}, "
              f"shade bytes {colors['shade_bytes']}, Gouraud bytes {colors['gouraud_bytes']}")
    elif colors.get("mode", "off") != "off":
        print(f"face colors: not used ({colors.get('reason')})")
    quads = report.get("quad_merge", {})
    if quads.get("enabled"):
        print(f"quad merge: {quads['source_triangles']} triangles -> {quads['polygons']} faces "
              f"({quads['merged_quads']} quads, {quads['single_triangles']} triangles)")
    weld = report.get("vertex_weld", {})
    if weld.get("enabled"):
        print(f"vertex weld: {weld['vertices_before']} -> {weld['vertices_after']} runtime vertices")
    print(f"unique textures: {report['textures']['unique_textures']}")
    print(f"texture VRAM estimate: {report['textures']['estimated_vram_bytes']}")
    vdp1 = report["vdp1"]
    print(f"VDP1 commands: model {vdp1['worst_case_model_commands']} + "
          f"reserved {vdp1['reserved_commands']} = {vdp1['total_command_estimate']} "
          f"(headroom {vdp1['command_headroom']})")
    print(f"RESULT: {'PASS' if report['result']['pass'] else 'FAIL'} saturn-vdp1 profile")


def main() -> int:
    parser = argparse.ArgumentParser(description="Import textured/animated 3D models for Saturn")
    parser.add_argument("--input", required=True, help="Input OBJ or GLB/GLTF file")
    parser.add_argument("--out-prefix", required=True, help="Output C/H prefix")
    parser.add_argument("--symbol", default=None, help="Generated symbol prefix (default: out-prefix name)")
    parser.add_argument("--scale", type=float, default=1.0, help="Uniform geometry scale")
    parser.add_argument("--flip-x", action="store_true")
    parser.add_argument("--flip-y", action="store_true")
    parser.add_argument("--flip-z", action="store_true")
    parser.add_argument("--reverse-winding", action="store_true",
                        help="Keep source winding instead of converting to LibSaturn clockwise")
    parser.add_argument("--palette-index", type=int, default=0)
    parser.add_argument("--max-texture-width", type=int, default=VDP1_MAX_TEXTURE_WIDTH)
    parser.add_argument("--max-texture-height", type=int, default=VDP1_MAX_TEXTURE_HEIGHT)
    parser.add_argument("--texture-scale", type=float, default=1.0,
                        help="Global baked-texture resolution scale")
    parser.add_argument("--sampling", default="nearest", choices=("nearest", "area"),
                        help="Bake sampling: nearest source texel, or area (box-filtered "
                             "average of the source each baked texel covers)")
    parser.add_argument("--lut-codes", default=None, metavar="LO-HI",
                        help="Animated GLB, lut4: tables hold VDP2 palette codes LO..HI of "
                             "one shared palette instead of RGB (8-bit/pixel hi-res "
                             "framebuffers)")
    parser.add_argument("--texel-extent", type=float, default=None,
                        help="Animated GLB: cap each face's baked texels at its world size, "
                             "scaled so the model's longest extent spans N texels (match "
                             "the on-screen size; pair with --sampling area)")
    parser.add_argument("--target", default=None,
                        help="Compilation target; 'saturn' enables the animated GLB path")
    parser.add_argument("--simplify", default="auto",
                        help="Animated GLB topology: off|auto|TARGET (default auto)")
    parser.add_argument("--quality", default="balanced",
                        help="Quality preset: conservative|balanced|aggressive")
    parser.add_argument("--max-triangles", type=int, default=None)
    parser.add_argument("--max-vdp1-commands", type=int, default=None)
    parser.add_argument("--max-pose-stream-bytes", type=int, default=256 * 1024,
                        help="Maximum baked animation data stored in Saturn RAM (default: 262144)")
    parser.add_argument("--hud-reserve", type=int, default=128,
                        help="VDP1 command budget reserved for this example's HUD (default: 128)")
    parser.add_argument("--animation", default="all",
                        help="Animated clips: all|NAME|INDEX (default all)")
    parser.add_argument("--animation-fps", default="source",
                        help="Runtime clip sampling: source|N fps (default source)")
    parser.add_argument("--merge-rigid-meshes", action="store_true",
                        help="Merge unskinned animated mesh nodes into one synthetic rigid skin")
    parser.add_argument("--generate-lods", action="store_true", default=False)
    parser.add_argument("--silhouette-views", type=int, default=16)
    parser.add_argument("--animation-weight", type=float, default=1.0)
    parser.add_argument("--silhouette-weight", type=float, default=1.0)
    parser.add_argument("--face-colors", default="off",
                        help="Animated GLB faces as solid lit colors instead of textures: "
                             "off|auto|on (default off)")
    parser.add_argument("--light-dir", default="-0.5,0.6,0.8",
                        help="Baked light direction x,y,z in asset space (towards the light)")
    parser.add_argument("--ambient", type=float, default=0.35,
                        help="Baked light floor for --face-colors (linear)")
    parser.add_argument("--diffuse", type=float, default=0.75,
                        help="Baked directional light strength for --face-colors (linear)")
    parser.add_argument("--merge-quads", default="off", choices=("off", "on"),
                        help="Animated GLB: draw adjacent triangle pairs as one VDP1 quad "
                             "when the texture mapping and fold allow it (default off)")
    parser.add_argument("--quad-max-texel-error", type=float, default=1.0,
                        help="Largest baked-texel shift a merged quad may introduce (default 1.0)")
    parser.add_argument("--quad-max-fold-deg", type=float, default=30.0,
                        help="Largest fold between merged triangles in any frame (default 30)")
    parser.add_argument("--locality-order", default="off", choices=("off", "on"),
                        help="Animated GLB, textured: order faces along the longest axis and "
                             "vertices by first use, so a split face list touches split "
                             "vertex ranges (default off)")
    parser.add_argument("--texture-format", default="indexed8", choices=("indexed8", "lut4"),
                        help="Animated GLB textures: indexed8 (one shared 256-color bank) or "
                             "lut4 (4 bits per texel, 15 colors per face in VDP1 lookup "
                             "tables: half the VRAM) (default indexed8)")
    parser.add_argument("--weld-vertices", default="off", choices=("off", "on"),
                        help="Animated GLB, textured: store one runtime vertex per point "
                             "that moves identically in every frame, dropping UV-split "
                             "copies (default off)")
    parser.add_argument("--material-weight", action="append", default=[],
                        help="Animated GLB: NAME=W or INDEX=W simplification cost weight for "
                             "one material; below 1 spends fewer triangles on it (repeatable)")
    parser.add_argument("--report", default=None, help="JSON report path (animated path)")
    parser.add_argument("--incremental", action="store_true",
                        help="Skip import when inputs, options, tools and outputs match")
    parser.add_argument("--force-import", action="store_true",
                        help="Regenerate even when --incremental finds a cache hit")
    parser.add_argument("--signature-only", action="store_true",
                        help="Update a signature stamp without importing the model")
    parser.add_argument("--signature-file", default=None,
                        help="Signature stamp path used with --signature-only")
    args = parser.parse_args()

    suffix = Path(args.input).suffix.lower()
    if suffix not in (".obj", ".glb", ".gltf"):
        print(f"import_model: error: expected .obj, .glb or .gltf (got {args.input})",
              file=sys.stderr)
        return 1
    if args.signature_only and not args.signature_file:
        print("import_model: error: --signature-only needs --signature-file", file=sys.stderr)
        return 1
    if args.signature_file and not args.signature_only:
        print("import_model: error: --signature-file requires --signature-only", file=sys.stderr)
        return 1
    if args.signature_only and (args.incremental or args.force_import):
        print("import_model: error: --signature-only cannot be combined with import mode flags",
              file=sys.stderr)
        return 1

    signature = None
    if args.signature_only or args.incremental or args.force_import:
        try:
            signature = _import_signature(args)
        except OSError as exc:
            print(f"import_model: error: cannot fingerprint inputs: {exc}", file=sys.stderr)
            return 1
        if args.signature_only:
            stamp = Path(args.signature_file)
            cache_valid = _incremental_cache_hit(args, signature)
            _atomic_write_json(stamp, {
                "schema_version": IMPORT_SIGNATURE_SCHEMA,
                "signature": signature["signature"],
            }, preserve_if_equal=cache_valid)
            if cache_valid:
                output_times = [path.stat().st_mtime_ns
                                for path in _model_output_paths(args).values()]
                if output_times:
                    safe_mtime = max(0, min(output_times) - 2_000_000_000)
                    os.utime(stamp, ns=(safe_mtime, safe_mtime))
            print(f"SIGNATURE: {signature['signature']} "
                  f"({'cache valid' if cache_valid else 'cache miss'})")
            return 0
        if (args.incremental and not args.force_import and
                _incremental_cache_hit(args, signature)):
            paths = _model_output_paths(args)
            print(f"UP-TO-DATE: {paths['source']} + {paths['header']}")
            return 0

    if suffix in (".glb", ".gltf"):
        result_code = _main_animated(args)
        if result_code == 0 and args.incremental and signature is not None:
            _write_incremental_manifest(args, signature)
        return result_code
    if args.simplify != "off" and (args.target == "saturn" or args.quality != "balanced"):
        print("import_model: error: --simplify/--quality apply to the animated GLB path; "
              "OBJ import is not simplified", file=sys.stderr)
        return 1
    try:
        result = import_model(
            obj_path=Path(args.input),
            scale=args.scale,
            flip_x=args.flip_x,
            flip_y=args.flip_y,
            flip_z=args.flip_z,
            reverse_winding=args.reverse_winding,
            palette_index=args.palette_index,
            max_texture_width=args.max_texture_width,
            max_texture_height=args.max_texture_height,
            texture_scale=args.texture_scale,
            sampling=args.sampling,
        )
        header_path, source_path = emit_c_h(result, Path(args.out_prefix), args.symbol)
    except ImportError as exc:
        print(f"import_model: error: {exc}", file=sys.stderr)
        return 1

    print_stats(result.stats)
    print(f"OK: {header_path} + {source_path}")
    if args.incremental and signature is not None:
        _write_incremental_manifest(args, signature)
    return 0


def _parse_lut_codes(text):
    if text is None:
        return None
    try:
        lo, hi = (int(v) for v in text.split("-"))
    except ValueError:
        raise ImportError(f"--lut-codes must be LO-HI (got {text!r})")
    return lo, hi


def _main_animated(args) -> int:
    if args.target not in (None, "saturn"):
        print(f"import_model: error: unknown --target {args.target!r} (have: saturn)",
              file=sys.stderr)
        return 1
    if args.simplify == "off" and args.generate_lods:
        print("import_model: error: --generate-lods needs simplification enabled",
              file=sys.stderr)
        return 1
    try:
        result = import_animated_model(
            glb_path=Path(args.input),
            scale=args.scale,
            flip_x=args.flip_x,
            flip_y=args.flip_y,
            flip_z=args.flip_z,
            reverse_winding=args.reverse_winding,
            palette_index=args.palette_index,
            max_texture_width=args.max_texture_width,
            max_texture_height=args.max_texture_height,
            texture_scale=args.texture_scale,
            sampling=args.sampling,
            texel_extent=args.texel_extent,
            lut_codes=_parse_lut_codes(args.lut_codes),
            simplify=args.simplify,
            quality=args.quality,
            max_triangles=args.max_triangles,
            max_vdp1_commands=args.max_vdp1_commands,
            max_pose_stream_bytes=args.max_pose_stream_bytes,
            hud_reserve=args.hud_reserve,
            animation=args.animation,
            animation_fps=args.animation_fps,
            merge_rigid_meshes=args.merge_rigid_meshes,
            generate_lods=args.generate_lods,
            silhouette_views=args.silhouette_views,
            animation_weight=args.animation_weight,
            silhouette_weight=args.silhouette_weight,
            face_colors=args.face_colors,
            light_dir=args.light_dir,
            ambient=args.ambient,
            diffuse=args.diffuse,
            merge_quads=args.merge_quads == "on",
            quad_max_texel_error=args.quad_max_texel_error,
            quad_max_fold_deg=args.quad_max_fold_deg,
            material_weights=args.material_weight,
            weld_vertices=args.weld_vertices == "on",
            texture_format=args.texture_format,
            locality_order=args.locality_order == "on",
        )
        header_path, source_path = emit_anim.emit_animated_c_h(
            result.static, result.animations, Path(args.out_prefix), args.symbol)
    except ImportError as exc:
        print(f"import_model: error: {exc}", file=sys.stderr)
        return 1
    except pose_bake.PoseBakeError as exc:
        print(f"import_model: error: {exc}", file=sys.stderr)
        return 1
    except gltf_mod.GltfError as exc:
        print(f"import_model: error: {exc}", file=sys.stderr)
        return 1

    if args.report:
        report_path = Path(args.report)
        report_path.parent.mkdir(parents=True, exist_ok=True)
        payload = dict(result.report)
        payload["output"] = {"header": str(header_path), "source": str(source_path)}
        c_bytes = source_path.read_bytes()
        h_bytes = header_path.read_bytes()
        payload["output"]["sha256_c"] = hashlib.sha256(c_bytes).hexdigest()
        payload["output"]["sha256_h"] = hashlib.sha256(h_bytes).hexdigest()
        report_path.write_text(json.dumps(payload, indent=2, sort_keys=True), encoding="utf-8")
    print_animated_stats(result.report)
    print(f"OK: {header_path} + {source_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
