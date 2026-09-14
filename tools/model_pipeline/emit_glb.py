#!/usr/bin/env python3
"""Simplified-GLB emission for desktop preview (host-only).

Rebuilds a valid, deterministic ``.glb`` from a simplified subset: the node
hierarchy, skins, inverse bind matrices, animation channels, materials and
embedded images are preserved verbatim (re-buffered), while vertex
attributes and indices carry the compacted subset, split into one primitive
per source material. Validators and desktop viewers accept the output; the
Saturn runtime never sees it (the Saturn path uses baked C/H assets).

Output is deterministic: identical input/options produce byte-identical
GLB (compact JSON with sorted keys, aligned binary chunk).
"""

from __future__ import annotations

import copy
import json
import struct

from .gltf import GltfError


def _align4(buf: bytearray) -> int:
    pad = (-len(buf)) % 4
    buf.extend(b"\x00" * pad)
    return len(buf)


class _Builder:
    def __init__(self):
        self.bin = bytearray()
        self.views: list[dict] = []
        self.accessors: list[dict] = []

    def add_floats(self, rows, n_comp, target=None, minimum=None, maximum=None):
        flat: list[float] = []
        for r in rows:
            flat.extend(float(x) for x in (r if isinstance(r, (list, tuple)) else (r,)))
        off = _align4(self.bin)
        self.bin.extend(struct.pack("<%df" % len(flat), *flat) if flat else b"")
        view = {"buffer": 0, "byteOffset": off, "byteLength": len(flat) * 4}
        if target is not None:
            view["target"] = target
        self.views.append(view)
        acc: dict = {
            "bufferView": len(self.views) - 1,
            "componentType": 5126,
            "count": len(rows),
            "type": {1: "SCALAR", 2: "VEC2", 3: "VEC3", 4: "VEC4", 16: "MAT4"}[n_comp],
        }
        if minimum is not None:
            acc["min"] = list(minimum)
            acc["max"] = list(maximum)
        self.accessors.append(acc)
        return len(self.accessors) - 1

    def add_u16(self, values, target=None):
        off = _align4(self.bin)
        self.bin.extend(struct.pack("<%dH" % len(values), *[int(v) for v in values]))
        self.views.append({"buffer": 0, "byteOffset": off, "byteLength": len(values) * 2, **({"target": target} if target is not None else {})})
        self.accessors.append({"bufferView": len(self.views) - 1, "componentType": 5123, "count": len(values) // 1 if target != 34962 else len(values), "type": "SCALAR" if target != 34962 else "VEC4"})
        return len(self.accessors) - 1

    def add_indices(self, values):
        use32 = max(values, default=0) > 65535
        off = _align4(self.bin)
        if use32:
            self.bin.extend(struct.pack("<%dI" % len(values), *[int(v) for v in values]))
            comp, size = 5125, 4
        else:
            self.bin.extend(struct.pack("<%dH" % len(values), *[int(v) for v in values]))
            comp, size = 5123, 2
        self.views.append({"buffer": 0, "byteOffset": off, "byteLength": len(values) * size, "target": 34963})
        self.accessors.append({"bufferView": len(self.views) - 1, "componentType": comp, "count": len(values), "type": "SCALAR"})
        return len(self.accessors) - 1

    def add_raw(self, payload: bytes) -> int:
        """Store an opaque blob (image bytes); returns the view index."""
        off = _align4(self.bin)
        self.bin.extend(bytes(payload))
        self.views.append({"buffer": 0, "byteOffset": off, "byteLength": len(payload)})
        return len(self.views) - 1


def _minmax(rows, n):
    cols = list(zip(*rows)) if rows else [() for _ in range(n)]
    return [min(c) if c else 0.0 for c in cols], [max(c) if c else 0.0 for c in cols]


def emit_glb(source, simplified) -> bytes:
    """Build a simplified ``.glb`` preview from source + simplified subset."""
    from .gltf import get_buffer_view_bytes, read_accessor

    doc = source.json_doc
    if doc is None:
        raise GltfError("emit_glb needs source.json_doc (parse via model_pipeline.gltf)")
    b = _Builder()

    positions = [tuple(p) for p in simplified.positions]
    pos_min, pos_max = _minmax(positions, 3)
    attrs: dict = {
        "POSITION": b.add_floats(positions, 3, target=34962, minimum=pos_min, maximum=pos_max)
    }
    if simplified.normals is not None:
        normals = [tuple(n) for n in simplified.normals]
        n_min, n_max = _minmax(normals, 3)
        attrs["NORMAL"] = b.add_floats(normals, 3, target=34962, minimum=n_min, maximum=n_max)
    if simplified.uvs:
        attrs["TEXCOORD_0"] = b.add_floats([tuple(u) for u in simplified.uvs], 2, target=34962)
    if simplified.joints is not None:
        joints = [tuple(int(x) for x in j) for j in simplified.joints]
        if any(v < 0 or v > 65535 for j in joints for v in j):
            raise GltfError("emit_glb: joint index exceeds 16-bit range")
        flat = [v for j in joints for v in j]
        off = _align4(b.bin)
        b.bin.extend(struct.pack("<%dH" % len(flat), *flat))
        b.views.append({"buffer": 0, "byteOffset": off, "byteLength": len(flat) * 2, "target": 34962})
        b.accessors.append({"bufferView": len(b.views) - 1, "componentType": 5123, "count": len(joints), "type": "VEC4"})
        attrs["JOINTS_0"] = len(b.accessors) - 1
    if simplified.weights is not None:
        attrs["WEIGHTS_0"] = b.add_floats([tuple(w) for w in simplified.weights], 4, target=34962)

    # One primitive per source material, in first-appearance order.
    by_material: dict[int, list[int]] = {}
    for ti, mt in enumerate(simplified.tri_materials):
        by_material.setdefault(mt, []).append(ti)
    primitives = []
    for mt in sorted(by_material):
        flat_idx: list[int] = []
        for ti in by_material[mt]:
            flat_idx.extend(simplified.triangles[ti])
        prim: dict = dict(attrs)
        prim = {"attributes": dict(attrs), "indices": b.add_indices(flat_idx)}
        src_materials = doc.get("materials", [])
        if mt < len(src_materials):
            prim["material"] = mt
        primitives.append(prim)

    def _copy_accessor(old_index: int, memo: dict) -> int:
        if old_index in memo:
            return memo[old_index]
        data = read_accessor(source.glb, old_index)
        n_comp = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4, "MAT4": 16}[data.accessor_type]
        if data.component_type == 5126:
            new = b.add_floats([tuple(r) if isinstance(r, tuple) else (r,) for r in data.rows], n_comp)
        elif data.component_type in (5121, 5123, 5125):
            flat_vals = [int(v) for r in data.rows for v in (r if isinstance(r, tuple) else (r,))]
            fmt = {5121: "B", 5123: "H", 5125: "I"}[data.component_type]
            size = {5121: 1, 5123: 2, 5125: 4}[data.component_type]
            off = _align4(b.bin)
            b.bin.extend(struct.pack("<%d%s" % (len(flat_vals), fmt), *flat_vals))
            b.views.append({"buffer": 0, "byteOffset": off, "byteLength": len(flat_vals) * size})
            b.accessors.append({"bufferView": len(b.views) - 1, "componentType": data.component_type, "count": data.count,
                                "type": data.accessor_type})
            new = len(b.accessors) - 1
        else:
            raise GltfError(f"emit_glb: cannot copy componentType {data.component_type}")
        memo[old_index] = new
        return new

    memo: dict[int, int] = {}
    skin_doc = doc.get("skins", [])[source.skin_index] if source.skins else None
    ibm_acc = None
    if skin_doc is not None and "inverseBindMatrices" in skin_doc:
        src_ibm = read_accessor(source.glb, int(skin_doc["inverseBindMatrices"]))
        ibm_acc = b.add_floats([tuple(r) for r in src_ibm.rows], 16)

    animations = []
    for anim in doc.get("animations", []):
        samplers = [{
            "input": _copy_accessor(int(s["input"]), memo),
            "output": _copy_accessor(int(s["output"]), memo),
            "interpolation": s.get("interpolation", "LINEAR"),
        } for s in anim.get("samplers", [])]
        channels = [{"sampler": int(c["sampler"]), "target": dict(c["target"])} for c in anim.get("channels", [])]
        entry: dict = {"samplers": samplers, "channels": channels}
        if "name" in anim:
            entry["name"] = anim["name"]
        animations.append(entry)

    images = []
    for img in doc.get("images", []):
        if "bufferView" not in img:
            raise GltfError("emit_glb: only embedded bufferView images are supported")
        payload = get_buffer_view_bytes(source.glb, int(img["bufferView"]))
        entry = {"bufferView": b.add_raw(payload)}
        if "mimeType" in img:
            entry["mimeType"] = img["mimeType"]
        if "name" in img:
            entry["name"] = img["name"]
        images.append(entry)

    skins = []
    for si, sdoc in enumerate(doc.get("skins", [])):
        entry = {"joints": [int(j) for j in sdoc.get("joints", [])]}
        if si == source.skin_index and ibm_acc is not None:
            entry["inverseBindMatrices"] = ibm_acc
        elif "inverseBindMatrices" in sdoc:
            entry["inverseBindMatrices"] = _copy_accessor(int(sdoc["inverseBindMatrices"]), memo)
        if "skeleton" in sdoc:
            entry["skeleton"] = int(sdoc["skeleton"])
        if "name" in sdoc:
            entry["name"] = sdoc["name"]
        skins.append(entry)

    # Nodes reference the single emitted mesh at index 0: rewrite mesh refs.
    nodes = copy.deepcopy(doc.get("nodes", []))
    for node in nodes:
        if "mesh" in node:
            node["mesh"] = 0

    out_doc: dict = {
        "asset": {"version": "2.0", "generator": "libsaturn-simplify_model"},
        "accessors": b.accessors,
        "bufferViews": b.views,
        "buffers": [{"byteLength": len(b.bin)}],
        "meshes": [{"primitives": primitives}],
        "nodes": nodes,
        "scenes": doc.get("scenes", [{"nodes": [0]}]),
        "scene": doc.get("scene", 0),
    }
    if skins:
        out_doc["skins"] = skins
    if animations:
        out_doc["animations"] = animations
    if doc.get("materials"):
        out_doc["materials"] = doc["materials"]
    if images:
        out_doc["images"] = images
    if doc.get("textures"):
        out_doc["textures"] = doc["textures"]
    if doc.get("samplers"):
        out_doc["samplers"] = doc["samplers"]

    raw_json = json.dumps(out_doc, sort_keys=True, separators=(",", ":")).encode("utf-8")
    while len(raw_json) % 4:
        raw_json += b" "
    total = 12 + 8 + len(raw_json) + 8 + len(b.bin)
    out = bytearray()
    out.extend(b"glTF")
    out.extend(struct.pack("<II", 2, total))
    out.extend(struct.pack("<I", len(raw_json)))
    out.extend(struct.pack("<I", 0x4E4F534A))
    out.extend(raw_json)
    out.extend(struct.pack("<I", len(b.bin)))
    out.extend(struct.pack("<I", 0x004E4942))
    out.extend(b.bin)
    return bytes(out)
