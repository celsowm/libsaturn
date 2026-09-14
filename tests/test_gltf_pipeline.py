#!/usr/bin/env python3
"""Tests for the animated-model GLB reader and host animation evaluator.

All fixtures are generated programmatically (tiny synthetic GLBs and analytic
skeletons), so no test depends on copyrighted acceptance assets.
"""

import io
import json
import math
import struct
import sys
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(REPO / "tools"))

from model_pipeline import animation as anim
from model_pipeline import gltf
from model_pipeline import model as model_mod
from model_pipeline.gltf import GltfError


# ----------------------------------------------------------------------
# Minimal in-memory GLB builder (permissively licensed test fixture)
# ----------------------------------------------------------------------


class GlbBuilder:
    """Assembles a minimal GLB from raw arrays, 4-byte aligned."""

    def __init__(self):
        self.bin = bytearray()
        self.doc = {
            "asset": {"version": "2.0"},
            "bufferViews": [],
            "accessors": [],
        }

    def add_bytes(self, payload: bytes, target=None, stride=None):
        while len(self.bin) % 4:
            self.bin.append(0)
        view = {"buffer": 0, "byteOffset": len(self.bin), "byteLength": len(payload)}
        if target is not None:
            view["target"] = target
        if stride is not None:
            view["byteStride"] = stride
        self.bin.extend(payload)
        self.doc["bufferViews"].append(view)
        return len(self.doc["bufferViews"]) - 1

    def add_floats(self, rows, n_comp, target=None, stride=None):
        flat = []
        for r in rows:
            vals = r if isinstance(r, (list, tuple)) else (r,)
            flat.extend(float(v) for v in vals)
        payload = struct.pack("<%df" % len(flat), *flat)
        return self.add_bytes(payload, target, stride)

    def add_u16(self, values, target=None):
        return self.add_bytes(struct.pack("<%dH" % len(values), *values), target)

    def add_u32(self, values, target=None):
        return self.add_bytes(struct.pack("<%dI" % len(values), *values), *(() if target is None else (target,)))

    def add_u8(self, values, target=None):
        return self.add_bytes(bytes(values), target)

    def add_accessor(self, view, comp, count, atype, byte_offset=0, normalized=False):
        acc = {
            "bufferView": view,
            "componentType": comp,
            "count": count,
            "type": atype,
        }
        if byte_offset:
            acc["byteOffset"] = byte_offset
        if normalized:
            acc["normalized"] = True
        self.doc["accessors"].append(acc)
        return len(self.doc["accessors"]) - 1

    def finish(self) -> bytes:
        self.doc.setdefault("buffers", [{"byteLength": len(self.bin)}])
        raw_json = json.dumps(self.doc).encode("utf-8")
        while len(raw_json) % 4:
            raw_json += b" "
        total = 12 + 8 + len(raw_json) + (8 + len(self.bin) if self.bin else 0)
        out = bytearray()
        out.extend(b"glTF")
        out.extend(struct.pack("<II", 2, total))
        out.extend(struct.pack("<I", len(raw_json)))
        out.extend(struct.pack("<I", 0x4E4F534A))
        out.extend(raw_json)
        if self.bin:
            out.extend(struct.pack("<I", len(self.bin)))
            out.extend(struct.pack("<I", 0x004E4942))
            out.extend(self.bin)
        return bytes(out)


def minimal_doc_builder():
    b = GlbBuilder()
    pos = b.add_floats([(0, 0, 0), (1, 0, 0), (0, 1, 0)], 3, target=34962)
    b.add_accessor(pos, 5126, 3, "VEC3")
    return b


# ----------------------------------------------------------------------
# Container tests
# ----------------------------------------------------------------------


class GlbContainerTests(unittest.TestCase):
    def test_valid_glb_round_trip(self):
        data = minimal_doc_builder().finish()
        parsed = gltf.parse_glb_bytes(data)
        self.assertEqual(parsed.json["asset"]["version"], "2.0")
        self.assertEqual(len(parsed.bin), 36)

    def test_bad_magic_rejected(self):
        data = bytearray(minimal_doc_builder().finish())
        data[0:4] = b"glTX"
        with self.assertRaises(GltfError) as ctx:
            gltf.parse_glb_bytes(bytes(data))
        self.assertIn("magic", str(ctx.exception))

    def test_truncated_header_rejected(self):
        with self.assertRaises(GltfError):
            gltf.parse_glb_bytes(b"glT")

    def test_truncated_chunk_rejected(self):
        data = minimal_doc_builder().finish()[:-4]
        with self.assertRaises(GltfError):
            gltf.parse_glb_bytes(data)

    def test_length_mismatch_rejected(self):
        data = bytearray(minimal_doc_builder().finish())
        struct.pack_into("<I", data, 8, len(data) - 1)
        with self.assertRaises(GltfError) as ctx:
            gltf.parse_glb_bytes(bytes(data))
        self.assertIn("disagrees", str(ctx.exception))

    def test_missing_json_rejected(self):
        # BIN chunk only.
        payload = b"1234"
        total = 12 + 8 + len(payload)
        data = b"glTF" + struct.pack("<II", 2, total)
        data += struct.pack("<II", len(payload), 0x004E4942) + payload
        with self.assertRaises(GltfError) as ctx:
            gltf.parse_glb_bytes(data)
        self.assertIn("no JSON", str(ctx.exception))

    def test_unknown_chunk_rejected(self):
        data = bytearray(minimal_doc_builder().finish())
        # Corrupt the JSON chunk type.
        struct.pack_into("<I", data, 16, 0xDEADBEEF)
        with self.assertRaises(GltfError):
            gltf.parse_glb_bytes(bytes(data))

    def test_wrong_suffix_rejected(self):
        with self.assertRaises(GltfError):
            gltf.parse_model(REPO / "README.md")


# ----------------------------------------------------------------------
# Accessor tests
# ----------------------------------------------------------------------


class AccessorTests(unittest.TestCase):
    def test_offset_and_stride_interleaved(self):
        # Interleaved POSITION+NORMAL: stride 24, accessors offset 0 and 12.
        b = GlbBuilder()
        inter = []
        for p, n in [((1, 2, 3), (0, 0, 1)), ((4, 5, 6), (0, 1, 0))]:
            inter.extend([*p, *n])
        view = b.add_bytes(struct.pack("<24f", *inter, *inter), target=34962, stride=24)
        pa = b.add_accessor(view, 5126, 2, "VEC3", byte_offset=0)
        na = b.add_accessor(view, 5126, 2, "VEC3", byte_offset=12)
        parsed = gltf.parse_glb_bytes(b.finish())
        pos = gltf.read_accessor(parsed, pa)
        nrm = gltf.read_accessor(parsed, na)
        self.assertEqual(pos.rows[0], (1.0, 2.0, 3.0))
        self.assertEqual(pos.rows[1], (4.0, 5.0, 6.0))
        self.assertEqual(nrm.rows[0], (0.0, 0.0, 1.0))
        self.assertEqual(nrm.rows[1], (0.0, 1.0, 0.0))

    def test_normalized_weights_ubyte(self):
        b = GlbBuilder()
        view = b.add_u8([255, 128, 0, 64])
        acc = b.add_accessor(view, 5121, 1, "VEC4", normalized=True)
        parsed = gltf.parse_glb_bytes(b.finish())
        data = gltf.read_accessor(parsed, acc)
        w = data.rows[0]
        self.assertAlmostEqual(w[0], 1.0)
        self.assertAlmostEqual(w[1], 128 / 255.0)
        self.assertAlmostEqual(w[2], 0.0)

    def test_u8_u16_and_u32_indices(self):
        b = GlbBuilder()
        v8 = b.add_u8([0, 1, 2])
        v16 = b.add_u16([0, 1, 2])
        v32 = b.add_u32([2, 1, 0])
        a8 = b.add_accessor(v8, 5121, 3, "SCALAR")
        a16 = b.add_accessor(v16, 5123, 3, "SCALAR")
        a32 = b.add_accessor(v32, 5125, 3, "SCALAR")
        parsed = gltf.parse_glb_bytes(b.finish())
        self.assertEqual(gltf.read_indices(parsed, a8), [0, 1, 2])
        self.assertEqual(gltf.read_indices(parsed, a16), [0, 1, 2])
        self.assertEqual(gltf.read_indices(parsed, a32), [2, 1, 0])

    def test_float_indices_rejected(self):
        b = GlbBuilder()
        view = b.add_floats([0.0, 1.0, 2.0], 1)
        acc = b.add_accessor(view, 5126, 3, "SCALAR")
        parsed = gltf.parse_glb_bytes(b.finish())
        with self.assertRaises(GltfError) as ctx:
            gltf.read_indices(parsed, acc)
        self.assertIn("5126", str(ctx.exception))

    def test_sparse_rejected(self):
        b = minimal_doc_builder()
        b.doc["accessors"][0]["sparse"] = {"count": 1, "indices": {}, "values": {}}
        parsed = gltf.parse_glb_bytes(b.finish())
        with self.assertRaises(GltfError) as ctx:
            gltf.read_accessor(parsed, 0)
        self.assertIn("sparse", str(ctx.exception))

    def test_out_of_range_view_rejected(self):
        b = minimal_doc_builder()
        parsed = gltf.parse_glb_bytes(b.finish())
        with self.assertRaises(GltfError):
            gltf.get_buffer_view_bytes(parsed, 99)


class ImageTests(unittest.TestCase):
    def _png_bytes(self):
        from PIL import Image

        img = Image.new("RGBA", (2, 2), (10, 20, 30, 255))
        img.putpixel((1, 1), (0, 0, 0, 0))
        buf = io.BytesIO()
        img.save(buf, format="PNG")
        return buf.getvalue()

    def test_embedded_png_extraction(self):
        png = self._png_bytes()
        b = GlbBuilder()
        view = b.add_bytes(png)
        b.doc["images"] = [{"bufferView": view, "mimeType": "image/png"}]
        parsed = gltf.parse_glb_bytes(b.finish())
        payload, mime = gltf.extract_image_bytes(parsed, 0)
        self.assertEqual(mime, "image/png")
        w, h, pixels = gltf.decode_image_rgba(payload)
        self.assertEqual((w, h), (2, 2))
        self.assertEqual(pixels[0], (10, 20, 30, 255))
        self.assertEqual(pixels[3], (0, 0, 0, 0))

    def test_external_image_rejected(self):
        b = GlbBuilder()
        b.doc["images"] = [{"uri": "tex.png"}]
        parsed = gltf.parse_glb_bytes(b.finish())
        with self.assertRaises(GltfError) as ctx:
            gltf.extract_image_bytes(parsed, 0)
        self.assertIn("bufferView", str(ctx.exception))


class NodeTransformTests(unittest.TestCase):
    def test_matrix_and_trs_agree(self):
        node_m = {"matrix": [0, 1, 0, 0, -1, 0, 0, 0, 0, 0, 1, 0, 5, 6, 7, 1]}
        node_t = {
            "translation": [5, 6, 7],
            "rotation": [0, 0, 0.7071068, 0.7071068],
            "scale": [1, 1, 1],
        }
        m = gltf.node_local_matrix(node_m)
        t = gltf.node_local_matrix(node_t)
        for a, b2 in zip(m, t):
            self.assertAlmostEqual(a, b2, places=5)

    def test_bad_matrix_rejected(self):
        with self.assertRaises(GltfError):
            gltf.node_local_matrix({"matrix": [1, 2, 3]})

    def test_unsupported_primitive_diagnostic(self):
        with self.assertRaises(GltfError) as ctx:
            gltf.check_primitive_mode({"mode": 0}, 3)
        self.assertIn("POINTS", str(ctx.exception))
        self.assertIn("mesh 3", str(ctx.exception))

    def test_cubicspline_diagnostic(self):
        with self.assertRaises(GltfError) as ctx:
            gltf.check_interpolation("CUBICSPLINE", 4)
        self.assertIn("sampler 4", str(ctx.exception))
        self.assertIn("CUBICSPLINE", str(ctx.exception))


# ----------------------------------------------------------------------
# Analytic evaluator tests (no assets at all)
# ----------------------------------------------------------------------


def analytic_model(joints, weights, nodes, clips=None, skin_joints=None):
    m = model_mod.SourceModel()
    m.vertices = [(1.0, 0.0, 0.0), (0.0, 0.0, 0.0)]
    m.normals = [(1.0, 0.0, 0.0), (0.0, 1.0, 0.0)]
    m.uvs = [(0.0, 0.0), (1.0, 1.0)]
    m.triangles = [(0, 1, 0)]
    m.tri_materials = [0]
    m.joints = joints
    m.weights = weights
    m.nodes = nodes
    joints_idx = skin_joints if skin_joints is not None else list(range(len(nodes)))
    m.skins = [
        model_mod.SourceSkin(
            joints=joints_idx,
            inverse_bind=[[1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1]] * len(joints_idx),
        )
    ]
    m.skin_index = 0
    m.clips = clips or []
    return m


def single_node(tx=0.0, ty=0.0, tz=0.0, rot=(0, 0, 0, 1)):
    return model_mod.SourceNode(name="j", translation=(tx, ty, tz), rotation=rot)


class EvaluatorTests(unittest.TestCase):
    def test_translated_joint_moves_vertex(self):
        m = analytic_model(
            [(0, 0, 0, 0), (0, 0, 0, 0)],
            [(1, 0, 0, 0), (1, 0, 0, 0)],
            [single_node(tx=2.0)],
        )
        out = anim.evaluate_positions(m, None, 0.0)
        self.assertAlmostEqual(out[0][0], 3.0)
        self.assertAlmostEqual(out[0][1], 0.0)

    def test_rotated_joint_rotates_vertex(self):
        s = math.sqrt(0.5)
        m = analytic_model(
            [(0, 0, 0, 0), (0, 0, 0, 0)],
            [(1, 0, 0, 0), (1, 0, 0, 0)],
            [single_node(rot=(0, 0, s, s))],  # 90 deg about Z
        )
        out = anim.evaluate_positions(m, None, 0.0)
        self.assertAlmostEqual(out[0][0], 0.0, places=5)
        self.assertAlmostEqual(out[0][1], 1.0, places=5)

    def test_two_weighted_joints_blend(self):
        m = analytic_model(
            [(0, 1, 0, 0), (0, 1, 0, 0)],
            [(0.5, 0.5, 0, 0), (0.5, 0.5, 0, 0)],
            [single_node(), single_node(tx=2.0)],
        )
        out = anim.evaluate_positions(m, None, 0.0)
        # vert0 (1,0,0): 0.5*1 + 0.5*3 = 2; vert1 (0,0,0): 0.5*0+0.5*2 = 1.
        self.assertAlmostEqual(out[0][0], 2.0)
        self.assertAlmostEqual(out[1][0], 1.0)

    def test_hierarchy_parent_child_compose(self):
        parent = model_mod.SourceNode(name="p", translation=(1, 0, 0), children=[1])
        child = model_mod.SourceNode(name="c", translation=(0, 2, 0))
        m = analytic_model(
            [(0, 0, 0, 0), (0, 0, 0, 0)],
            [(1, 0, 0, 0), (1, 0, 0, 0)],
            [parent, child],
            skin_joints=[1],
        )
        out = anim.evaluate_positions(m, None, 0.0)
        self.assertAlmostEqual(out[1][0], 1.0)
        self.assertAlmostEqual(out[1][1], 2.0)

    def test_quaternion_interpolation_midpoint(self):
        s = math.sqrt(0.5)
        clip = model_mod.AnimationClip(
            name="rot",
            duration=1.0,
            channels=[
                model_mod.AnimationChannel(
                    node=0,
                    path="rotation",
                    times=[0.0, 1.0],
                    values=[(0, 0, 0, 1), (0, 0, s, s)],
                    interpolation="LINEAR",
                )
            ],
        )
        m = analytic_model(
            [(0, 0, 0, 0)], [(1, 0, 0, 0)], [single_node()], clips=[clip],
            skin_joints=[0],
        )
        m.vertices = [(1.0, 0.0, 0.0)]
        out = anim.evaluate_positions(m, clip, 0.5)
        # Halfway between identity and 90 deg is a 45 deg rotation.
        c = math.cos(math.pi / 4)
        sn = math.sin(math.pi / 4)
        self.assertAlmostEqual(out[0][0], c, places=5)
        self.assertAlmostEqual(out[0][1], sn, places=5)

    def test_step_interpolation_holds(self):
        clip = model_mod.AnimationClip(
            name="step",
            duration=1.0,
            channels=[
                model_mod.AnimationChannel(
                    node=0,
                    path="translation",
                    times=[0.0, 1.0],
                    values=[(0, 0, 0), (10, 0, 0)],
                    interpolation="STEP",
                )
            ],
        )
        m = analytic_model(
            [(0, 0, 0, 0)], [(1, 0, 0, 0)], [single_node()], clips=[clip],
            skin_joints=[0],
        )
        m.vertices = [(0.0, 0.0, 0.0)]
        out = anim.evaluate_positions(m, clip, 0.5)
        self.assertAlmostEqual(out[0][0], 0.0)
        out = anim.evaluate_positions(m, clip, 1.0)
        self.assertAlmostEqual(out[0][0], 10.0)

    def test_loop_duplicate_terminal_removed(self):
        clip = model_mod.AnimationClip(
            name="loop",
            duration=1.0,
            channels=[
                model_mod.AnimationChannel(
                    node=0,
                    path="translation",
                    times=[0.0, 0.5, 1.0],
                    values=[(0, 0, 0), (0, 1, 0), (0, 0, 0)],
                    interpolation="LINEAR",
                )
            ],
        )
        m = analytic_model(
            [(0, 0, 0, 0)], [(1, 0, 0, 0)], [single_node()], clips=[clip],
            skin_joints=[0],
        )
        m.vertices = [(0.0, 0.0, 0.0)]
        times, removed = anim.runtime_frame_times(m, clip)
        self.assertTrue(removed)
        self.assertEqual(times, [0.0, 0.5])

    def test_non_loop_terminal_kept(self):
        clip = model_mod.AnimationClip(
            name="once",
            duration=1.0,
            channels=[
                model_mod.AnimationChannel(
                    node=0,
                    path="translation",
                    times=[0.0, 1.0],
                    values=[(0, 0, 0), (0, 5, 0)],
                    interpolation="LINEAR",
                )
            ],
        )
        m = analytic_model(
            [(0, 0, 0, 0)], [(1, 0, 0, 0)], [single_node()], clips=[clip],
            skin_joints=[0],
        )
        m.vertices = [(0.0, 0.0, 0.0)]
        times, removed = anim.runtime_frame_times(m, clip)
        self.assertFalse(removed)
        self.assertEqual(times, [0.0, 1.0])

    def test_inverse_bind_applied(self):
        # Inverse bind translating -1x cancels a joint translating +1x.
        ibm = [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, -1, 0, 0, 1]
        m = analytic_model(
            [(0, 0, 0, 0)], [(1, 0, 0, 0)],
            [single_node(tx=1.0)], skin_joints=[0],
        )
        m.skins[0].inverse_bind = [ibm]
        m.vertices = [(0.0, 0.0, 0.0)]
        out = anim.evaluate_positions(m, None, 0.0)
        self.assertAlmostEqual(out[0][0], 0.0)

    def test_skinned_normals_evaluated(self):
        s = math.sqrt(0.5)
        m = analytic_model(
            [(0, 0, 0, 0)], [(1, 0, 0, 0)],
            [single_node(rot=(0, 0, s, s))], skin_joints=[0],
        )
        n = anim.evaluate_normals(m, None, 0.0)
        assert n is not None
        self.assertAlmostEqual(n[0][0], 0.0, places=5)
        self.assertAlmostEqual(n[0][1], 1.0, places=5)


# ----------------------------------------------------------------------
# from_gltf integration over a tiny skinned GLB
# ----------------------------------------------------------------------


def build_skinned_gltf():
    from PIL import Image

    b = GlbBuilder()
    pos = b.add_floats([(0, 0, 0), (1, 0, 0), (0, 1, 0)], 3, target=34962)
    nrm = b.add_floats([(0, 0, 1)] * 3, 3, target=34962)
    uv = b.add_floats([(0, 0), (1, 0), (0, 1)], 2, target=34962)
    jnt = b.add_u16([0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0])
    wgt = b.add_floats([(1, 0, 0, 0), (0.5, 0.5, 0, 0), (0.25, 0.25, 0.25, 0.25)], 4)
    idx = b.add_u16([0, 1, 2])
    ibm = b.add_floats(
        [[1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1]] * 2, 16
    )
    img = Image.new("RGBA", (2, 2), (255, 0, 0, 255))
    buf = io.BytesIO()
    img.save(buf, format="PNG")
    img_view = b.add_bytes(buf.getvalue())
    times = b.add_floats([0.0, 1.0], 1)
    tout = b.add_floats([(0, 0, 0), (0, 1, 0)], 3)
    pa = b.add_accessor(pos, 5126, 3, "VEC3")
    na = b.add_accessor(nrm, 5126, 3, "VEC3")
    ua = b.add_accessor(uv, 5126, 3, "VEC2")
    ja = b.add_accessor(jnt, 5123, 3, "VEC4")
    wa = b.add_accessor(wgt, 5126, 3, "VEC4")
    ia = b.add_accessor(idx, 5123, 3, "SCALAR")
    ba = b.add_accessor(ibm, 5126, 2, "MAT4")
    ta = b.add_accessor(times, 5126, 2, "SCALAR")
    oa = b.add_accessor(tout, 5126, 2, "VEC3")
    b.doc["images"] = [{"bufferView": img_view, "mimeType": "image/png"}]
    b.doc["textures"] = [{"source": 0}]
    b.doc["materials"] = [
        {"name": "red", "pbrMetallicRoughness": {"baseColorTexture": {"index": 0}}}
    ]
    b.doc["meshes"] = [
        {
            "primitives": [
                {
                    "attributes": {
                        "POSITION": pa,
                        "NORMAL": na,
                        "TEXCOORD_0": ua,
                        "JOINTS_0": ja,
                        "WEIGHTS_0": wa,
                    },
                    "indices": ia,
                    "material": 0,
                }
            ]
        }
    ]
    b.doc["nodes"] = [
        {"name": "meshnode", "mesh": 0, "skin": 0},
        {"name": "j0", "translation": [0, 0, 0]},
        {"name": "j1", "translation": [1, 0, 0]},
    ]
    b.doc["skins"] = [{"joints": [1, 2], "inverseBindMatrices": ba}]
    b.doc["animations"] = [
        {
            "name": "bounce",
            "samplers": [{"input": ta, "output": oa, "interpolation": "LINEAR"}],
            "channels": [{"sampler": 0, "target": {"node": 1, "path": "translation"}}],
        }
    ]
    return b.finish()


class FromGltfTests(unittest.TestCase):
    def test_skinned_model_imports(self):
        parsed = gltf.parse_glb_bytes(build_skinned_gltf())
        m = model_mod.from_gltf(parsed, "tiny")
        self.assertEqual(len(m.vertices), 3)
        self.assertEqual(m.triangles, [(0, 1, 2)])
        self.assertEqual(len(m.textures), 1)
        self.assertEqual((m.textures[0].width, m.textures[0].height), (2, 2))
        self.assertEqual(m.joints[0], (0, 0, 0, 0))
        # Weights renormalize deterministically (row 2 sums to 1 already).
        self.assertAlmostEqual(sum(m.weights[1]), 1.0)
        self.assertEqual(len(m.skins[0].joints), 2)
        self.assertEqual(len(m.clips), 1)
        self.assertEqual(m.clips[0].duration, 1.0)
        stats = model_mod.source_stats(m)
        self.assertEqual(stats["vertices"], 3)
        self.assertEqual(stats["joints"], 2)

    def test_animation_evaluates_on_imported_model(self):
        parsed = gltf.parse_glb_bytes(build_skinned_gltf())
        m = model_mod.from_gltf(parsed, "tiny")
        p0 = anim.evaluate_positions(m, m.clips[0], 0.0)
        p1 = anim.evaluate_positions(m, m.clips[0], 1.0)
        # Joint 0 (node 1) rises +1y; vert0 is fully bound to it.
        self.assertAlmostEqual(p0[0][1], 0.0)
        self.assertAlmostEqual(p1[0][1], 1.0)

    def test_missing_uv_diagnostic(self):
        data = bytearray(build_skinned_gltf())
        parsed = gltf.parse_glb_bytes(bytes(data))
        # Drop TEXCOORD_0 from the primitive.
        prim = parsed.json["meshes"][0]["primitives"][0]
        del prim["attributes"]["TEXCOORD_0"]
        with self.assertRaises(GltfError) as ctx:
            model_mod.from_gltf(parsed, "tiny")
        self.assertIn("TEXCOORD_0", str(ctx.exception))

    def test_invalid_joint_diagnostic(self):
        parsed = gltf.parse_glb_bytes(build_skinned_gltf())
        # Point the skin at a nonexistent node.
        parsed.json["skins"][0]["joints"] = [1, 99]
        with self.assertRaises(GltfError) as ctx:
            model_mod.from_gltf(parsed, "tiny")
        self.assertIn("joint", str(ctx.exception))

    def test_zero_weight_sum_diagnostic(self):
        parsed = gltf.parse_glb_bytes(build_skinned_gltf())
        m = model_mod.SourceModel()
        with self.assertRaises(GltfError):
            model_mod._normalize_weights([(0, 0, 0, 0)], [(0, 0, 0, 0)], 1)

    def test_weights_renormalize(self):
        js, ws = model_mod._normalize_weights(
            [(0, 1, 0, 0)], [(0.25, 0.25, 0, 0)], 2
        )
        self.assertAlmostEqual(sum(ws[0]), 1.0)
        self.assertAlmostEqual(ws[0][0], 0.5)

    def test_multiple_skins_rejected(self):
        parsed = gltf.parse_glb_bytes(build_skinned_gltf())
        parsed.json["skins"].append({"joints": [1]})
        parsed.json["nodes"].append({"name": "mesh2", "mesh": 0, "skin": 1})
        with self.assertRaises(GltfError) as ctx:
            model_mod.from_gltf(parsed, "tiny")
        self.assertIn("skin", str(ctx.exception).lower())


if __name__ == "__main__":
    unittest.main()
