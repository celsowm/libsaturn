#!/usr/bin/env python3
"""Canonical host-only source-model representation.

Both the OBJ importer (static textured pipeline) and the glTF importer
(animated pipeline) converge here where practical, so simplification,
quality validation and Saturn compilation operate on one model shape
instead of two incompatible ones.

Saturn-specific A/B/C/D quad conventions are deliberately absent: parsing
produces plain triangles, and the Saturn compiler stage decides winding and
degenerate-quad packing. Material boundaries and UV seams are preserved
explicitly so simplification can protect them.
"""

from __future__ import annotations

from dataclasses import dataclass, field

from .gltf import (
    GltfError,
    check_primitive_mode,
    decode_image_rgba,
    extract_image_bytes,
    node_local_matrix,
    read_accessor,
    read_indices,
)


@dataclass
class SourceTexture:
    width: int
    height: int
    pixels_rgba: list  # [(r, g, b, a), ...] row-major
    mime: str = "image/png"


@dataclass
class SourceNode:
    name: str
    children: list[int] = field(default_factory=list)
    translation: tuple[float, float, float] = (0.0, 0.0, 0.0)
    rotation: tuple[float, float, float, float] = (0.0, 0.0, 0.0, 1.0)
    scale: tuple[float, float, float] = (1.0, 1.0, 1.0)
    matrix: list[float] | None = None  # column-major override when authored


@dataclass
class SourceSkin:
    joints: list[int] = field(default_factory=list)
    inverse_bind: list[list[float]] = field(default_factory=list)  # MAT4 each


@dataclass
class AnimationChannel:
    node: int
    path: str  # translation | rotation | scale
    times: list[float] = field(default_factory=list)
    values: list = field(default_factory=list)  # VEC3 / VEC4 rows
    interpolation: str = "LINEAR"


@dataclass
class AnimationClip:
    name: str
    duration: float = 0.0
    channels: list[AnimationChannel] = field(default_factory=list)


@dataclass
class SourceModel:
    """Host-only source representation shared by OBJ and glTF importers."""

    vertices: list[tuple[float, float, float]] = field(default_factory=list)
    normals: list[tuple[float, float, float]] | None = None
    uvs: list[tuple[float, float]] = field(default_factory=list)
    triangles: list[tuple[int, int, int]] = field(default_factory=list)
    tri_materials: list[int] = field(default_factory=list)
    materials: list[dict] = field(default_factory=list)
    textures: list[SourceTexture] = field(default_factory=list)
    joints: list[tuple[int, int, int, int]] = field(default_factory=list)
    weights: list[tuple[float, float, float, float]] = field(default_factory=list)
    nodes: list[SourceNode] = field(default_factory=list)
    skins: list[SourceSkin] = field(default_factory=list)
    clips: list[AnimationClip] = field(default_factory=list)
    mesh_node: int = -1  # node carrying the merged mesh (for rest transform)
    skin_index: int = -1

    @property
    def is_skinned(self) -> bool:
        return bool(self.joints and self.weights and self.skins)

    def bbox(self) -> tuple[tuple[float, float, float], tuple[float, float, float]]:
        if not self.vertices:
            raise GltfError("source model has no vertices")
        xs = [v[0] for v in self.vertices]
        ys = [v[1] for v in self.vertices]
        zs = [v[2] for v in self.vertices]
        return (min(xs), min(ys), min(zs)), (max(xs), max(ys), max(zs))

    def bbox_diagonal(self) -> float:
        import math

        lo, hi = self.bbox()
        return math.sqrt(
            (hi[0] - lo[0]) ** 2 + (hi[1] - lo[1]) ** 2 + (hi[2] - lo[2]) ** 2
        )


def _as_v3(row, what: str) -> tuple[float, float, float]:
    if len(row) != 3:
        raise GltfError(f"{what}: expected VEC3, got {len(row)} components")
    return (float(row[0]), float(row[1]), float(row[2]))


def _as_v2(row, what: str) -> tuple[float, float]:
    if len(row) != 2:
        raise GltfError(f"{what}: expected VEC2, got {len(row)} components")
    return (float(row[0]), float(row[1]))


def _normalize_weights(
    joints: list, weights: list, joint_count: int
) -> tuple[list[tuple[int, int, int, int]], list[tuple[float, float, float, float]]]:
    """Validate joint indices and deterministically normalize skin weights."""
    out_j, out_w = [], []
    for vi, (j_row, w_row) in enumerate(zip(joints, weights)):
        if len(j_row) != 4 or len(w_row) != 4:
            raise GltfError(f"vertex {vi}: JOINTS_0/WEIGHTS_0 must be VEC4")
        js = [int(v) for v in j_row]
        ws = [max(float(v), 0.0) for v in w_row]
        for j in js:
            if j < 0 or j >= joint_count:
                raise GltfError(
                    f"vertex {vi}: joint index {j} out of range ({joint_count} joints)"
                )
        total = sum(ws)
        if total <= 0.0:
            raise GltfError(f"vertex {vi}: skin weights sum to zero")
        # Vertices with fewer than four meaningful influences keep their
        # trailing slots at zero weight; normalization is over the sum so
        # they still deform exactly.
        ws = [w / total for w in ws]
        out_j.append((js[0], js[1], js[2], js[3]))
        out_w.append((ws[0], ws[1], ws[2], ws[3]))
    return out_j, out_w


def from_gltf(glb, source_name: str = "model") -> SourceModel:
    """Build a canonical source model from parsed GLB data.

    All mesh primitives merge into one triangle soup; material boundaries
    are recorded per triangle so simplification never merges across them.
    Only skinned triangle meshes with TEXCOORD_0 are accepted for the
    animated Saturn path; anything else fails with a precise diagnostic
    instead of rendering incorrectly.
    """
    from .gltf import GlbData  # noqa: F401  (type reference only)

    doc = glb.json
    model = SourceModel()

    meshes = doc.get("meshes", [])
    if not meshes:
        raise GltfError(f"{source_name}: glTF has no meshes")
    nodes = doc.get("nodes", [])
    for ni, node in enumerate(nodes):
        model.nodes.append(
            SourceNode(
                name=str(node.get("name", f"node_{ni}")),
                children=[int(c) for c in node.get("children", [])],
                translation=tuple(float(v) for v in node.get("translation", (0.0, 0.0, 0.0))),  # type: ignore[arg-type]
                rotation=tuple(float(v) for v in node.get("rotation", (0.0, 0.0, 0.0, 1.0))),  # type: ignore[arg-type]
                scale=tuple(float(v) for v in node.get("scale", (1.0, 1.0, 1.0))),  # type: ignore[arg-type]
                matrix=[float(v) for v in node["matrix"]] if "matrix" in node else None,
            )
        )

    # Locate the (single) skinned mesh node. Multiple distinct skins on one
    # primitive are rejected: blending across skeletons has no defined
    # meaning for the baked-pose runtime.
    skinned_nodes = [
        ni for ni, n in enumerate(nodes) if "mesh" in n and "skin" in n
    ]
    plain_nodes = [ni for ni, n in enumerate(nodes) if "mesh" in n and "skin" not in n]
    skins_doc = doc.get("skins", [])
    if skinned_nodes:
        skin_indices = {int(nodes[ni]["skin"]) for ni in skinned_nodes}
        if len(skin_indices) > 1:
            raise GltfError(
                f"{source_name}: multiple skins {sorted(skin_indices)} on one "
                "model are not supported (bake one skeleton per asset)"
            )
        model.skin_index = next(iter(skin_indices))
        if model.skin_index < 0 or model.skin_index >= len(skins_doc):
            raise GltfError(f"{source_name}: skin {model.skin_index} out of range")
        # Multiple mesh nodes sharing one skin would duplicate the surface;
        # keep the merge deterministic by requiring exactly one mesh node.
        mesh_ids = sorted({int(nodes[ni]["mesh"]) for ni in skinned_nodes})
        if len(skinned_nodes) > 1 or len(mesh_ids) > 1:
            raise GltfError(
                f"{source_name}: {len(skinned_nodes)} skinned mesh nodes found; "
                "only a single skinned mesh node is supported initially"
            )
        model.mesh_node = skinned_nodes[0]
    elif plain_nodes:
        mesh_ids = sorted({int(nodes[ni]["mesh"]) for ni in plain_nodes})
        if len(mesh_ids) > 1:
            raise GltfError(
                f"{source_name}: {len(mesh_ids)} meshes found; only a single "
                "mesh is supported initially"
            )
        model.mesh_node = plain_nodes[0]
    else:
        raise GltfError(f"{source_name}: no node carries a mesh")

    # Skins.
    for skin_doc in skins_doc:
        joints = [int(j) for j in skin_doc.get("joints", [])]
        if not joints:
            raise GltfError(f"{source_name}: skin has no joints")
        for j in joints:
            if j < 0 or j >= len(model.nodes):
                raise GltfError(f"{source_name}: skin joint node {j} out of range")
        ibm_acc = skin_doc.get("inverseBindMatrices")
        inverse = []
        if ibm_acc is not None:
            data = read_accessor(glb, int(ibm_acc))
            if data.accessor_type != "MAT4" or data.count != len(joints):
                raise GltfError(
                    f"{source_name}: inverseBindMatrices must be {len(joints)} MAT4 rows"
                )
            inverse = [[float(v) for v in row] for row in data.rows]
        else:
            inverse = [
                [1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0]
                for _ in joints
            ]
        model.skins.append(SourceSkin(joints=joints, inverse_bind=inverse))

    # Merge primitives.
    raw_joints: list = []
    raw_weights: list = []
    has_skin_data = False
    mesh_index = int(nodes[model.mesh_node]["mesh"])
    primitives = meshes[mesh_index].get("primitives", [])
    if not primitives:
        raise GltfError(f"{source_name}: mesh {mesh_index} has no primitives")
    for prim in primitives:
        check_primitive_mode(prim, mesh_index)
        attrs = prim.get("attributes", {})
        for need in ("POSITION", "TEXCOORD_0"):
            if need not in attrs:
                raise GltfError(
                    f"{source_name}: primitive is missing {need} "
                    "(the animated textured path needs UV-mapped positions)"
                )
        pos = read_accessor(glb, int(attrs["POSITION"]))
        if pos.accessor_type != "VEC3":
            raise GltfError(f"{source_name}: POSITION must be VEC3")
        base = len(model.vertices)
        model.vertices.extend(_as_v3(r, "POSITION") for r in pos.rows)
        if "NORMAL" in attrs:
            nrm = read_accessor(glb, int(attrs["NORMAL"]))
            if nrm.accessor_type != "VEC3":
                raise GltfError(f"{source_name}: NORMAL must be VEC3")
            if model.normals is None and model.vertices and len(model.vertices) != len(pos.rows):
                raise GltfError(f"{source_name}: NORMAL count disagrees with POSITION")
            if model.normals is None:
                model.normals = []
            model.normals.extend(_as_v3(r, "NORMAL") for r in nrm.rows)
        elif model.normals is not None:
            raise GltfError(f"{source_name}: mixed NORMAL presence across primitives")
        uv = read_accessor(glb, int(attrs["TEXCOORD_0"]))
        model.uvs.extend(_as_v2(r, "TEXCOORD_0") for r in uv.rows)
        if len(model.uvs) - len(uv.rows) + len(uv.rows) != len(model.vertices):
            pass  # counts checked below per primitive
        if len(uv.rows) != len(pos.rows):
            raise GltfError(f"{source_name}: TEXCOORD_0 count disagrees with POSITION")
        if "JOINTS_0" in attrs or "WEIGHTS_0" in attrs:
            if "JOINTS_0" not in attrs or "WEIGHTS_0" not in attrs:
                raise GltfError(
                    f"{source_name}: JOINTS_0 and WEIGHTS_0 must appear together"
                )
            has_skin_data = True
            jd = read_accessor(glb, int(attrs["JOINTS_0"]))
            wd = read_accessor(glb, int(attrs["WEIGHTS_0"]))
            if jd.accessor_type != "VEC4" or wd.accessor_type != "VEC4":
                raise GltfError(f"{source_name}: JOINTS_0/WEIGHTS_0 must be VEC4")
            if jd.count != len(pos.rows) or wd.count != len(pos.rows):
                raise GltfError(f"{source_name}: skin attribute count disagrees")
            raw_joints.extend(jd.rows)
            raw_weights.extend(wd.rows)
        if "indices" not in prim:
            raise GltfError(f"{source_name}: non-indexed primitives are not supported")
        indices = read_indices(glb, int(prim["indices"]))
        if len(indices) % 3 != 0:
            raise GltfError(f"{source_name}: index count is not a multiple of 3")
        for v in indices:
            if v < 0 or v >= len(pos.rows):
                raise GltfError(f"{source_name}: index {v} out of range")
        mat_index = 0
        if "material" in prim:
            mat_index = int(prim["material"])
            while len(model.materials) <= mat_index:
                model.materials.append({})
        for i in range(0, len(indices), 3):
            model.triangles.append(
                (base + indices[i], base + indices[i + 1], base + indices[i + 2])
            )
            model.tri_materials.append(mat_index)

    # Materials / textures.
    for mi, mat in enumerate(doc.get("materials", [])):
        while len(model.materials) <= mi:
            model.materials.append({})
        entry = model.materials[mi]
        entry["name"] = str(mat.get("name", f"material_{mi}"))
        entry["doubleSided"] = bool(mat.get("doubleSided", False))
        pbr = mat.get("pbrMetallicRoughness", {})
        tex_info = pbr.get("baseColorTexture")
        if tex_info is not None:
            tex_doc = doc.get("textures", [])[int(tex_info["index"])]
            img_index = int(tex_doc["source"])
            payload, mime = extract_image_bytes(glb, img_index)
            w, h, pixels = decode_image_rgba(payload, f"material {mi}")
            entry["texture"] = len(model.textures)
            model.textures.append(
                SourceTexture(width=w, height=h, pixels_rgba=pixels, mime=mime)
            )

    if model.skins:
        if not has_skin_data:
            raise GltfError(f"{source_name}: skinned mesh has no JOINTS_0/WEIGHTS_0")
        joint_count = len(model.skins[model.skin_index].joints)
        model.joints, model.weights = _normalize_weights(
            raw_joints, raw_weights, joint_count
        )
    elif has_skin_data:
        raise GltfError(f"{source_name}: skin attributes without a skin")

    # Animations.
    for ai, anim in enumerate(doc.get("animations", [])):
        name = str(anim.get("name", f"anim_{ai}"))
        samplers = anim.get("samplers", [])
        channels: list[AnimationChannel] = []
        duration = 0.0
        for ch in anim.get("channels", []):
            sampler = samplers[int(ch["sampler"])]
            target = ch["target"]
            node_idx = int(target["node"])
            if node_idx < 0 or node_idx >= len(model.nodes):
                raise GltfError(f"{source_name}: animation targets node {node_idx} out of range")
            path = str(target["path"])
            if path not in ("translation", "rotation", "scale"):
                raise GltfError(
                    f"{source_name}: animation path {path!r} is not supported "
                    "(only translation/rotation/scale; morph weights need a "
                    "blend-shape runtime the Saturn build does not have)"
                )
            from .gltf import check_interpolation

            interp = str(sampler.get("interpolation", "LINEAR"))
            check_interpolation(interp, int(ch["sampler"]))
            in_acc = read_accessor(glb, int(sampler["input"]))
            out_acc = read_accessor(glb, int(sampler["output"]))
            if in_acc.accessor_type != "SCALAR":
                raise GltfError(f"{source_name}: animation input must be SCALAR time")
            times = [float(v) for v in in_acc.rows]
            if any(b < a for a, b in zip(times, times[1:])):
                raise GltfError(f"{source_name}: animation input times must be sorted")
            want = "VEC3" if path in ("translation", "scale") else "VEC4"
            if out_acc.accessor_type != want:
                raise GltfError(
                    f"{source_name}: animation output for {path} must be {want}"
                )
            if len(times) != len(out_acc.rows):
                raise GltfError(f"{source_name}: animation input/output count mismatch")
            if times:
                duration = max(duration, max(times))
            channels.append(
                AnimationChannel(
                    node=node_idx,
                    path=path,
                    times=times,
                    values=[tuple(float(v) for v in r) for r in out_acc.rows],
                    interpolation=interp,
                )
            )
        # Deterministic channel order: by node, then path.
        channels.sort(key=lambda c: (c.node, c.path))
        model.clips.append(AnimationClip(name=name, duration=duration, channels=channels))

    return model


def source_stats(model: SourceModel) -> dict:
    """Diagnostic counts for reports (never algorithmic constants)."""
    joints = len(model.skins[model.skin_index].joints) if model.skins else 0
    return {
        "vertices": len(model.vertices),
        "triangles": len(model.triangles),
        "materials": len(model.materials),
        "textures": len(model.textures),
        "joints": joints,
        "animation_clips": len(model.clips),
    }
