"""Wavefront OBJ/MTL parsing and material texture resolution."""
from __future__ import annotations
from dataclasses import dataclass, field
from pathlib import Path
from .errors import ImportError

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

