#!/usr/bin/env python3
"""Tests for simplified-GLB emission and the standalone simplify_model CLI.

Fixtures are synthetic animated GLBs generated in-test; nothing depends on
copyrighted assets. The acceptance GLB is exercised only when present
locally (skipped otherwise).
"""

import io
import json
import struct
import subprocess
import sys
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(REPO / "tools"))

from model_pipeline import emit_glb as emit_glb_mod
from model_pipeline import gltf
from model_pipeline import model as model_mod
from model_pipeline import simplification as simp_mod


def build_grid_glb(nx=4, ny=4):
    """Small skinned animated GLB with an embedded texture (test-owned)."""
    from PIL import Image

    pos, nrm, uv, jnt, wgt, idx = [], [], [], [], [], []
    for j in range(ny + 1):
        for i in range(nx + 1):
            pos.append((float(i), float(j), 0.0))
            nrm.append((0.0, 0.0, 1.0))
            uv.append((i / nx, j / ny))
            left = i <= nx // 2
            jnt.append((0, 0, 0, 0) if left else (1, 0, 0, 0))
            wgt.append((1, 0, 0, 0))
    for j in range(ny):
        for i in range(nx):
            a = j * (nx + 1) + i
            b, c, d = a + 1, a + nx + 2, a + nx + 1
            idx += [a, b, c, a, c, d]
    img = Image.new("RGBA", (4, 4), (200, 40, 40, 255))
    buf = io.BytesIO()
    img.save(buf, format="PNG")
    png = buf.getvalue()

    blob = bytearray()
    views, accessors = [], []

    def push(payload, target=None):
        while len(blob) % 4:
            blob.append(0)
        views.append({"buffer": 0, "byteOffset": len(blob), "byteLength": len(payload), **({"target": target} if target is not None else {})})
        blob.extend(payload)
        return len(views) - 1

    def acc(view, comp, count, atype):
        accessors.append({"bufferView": view, "componentType": comp, "count": count, "type": atype})
        return len(accessors) - 1

    def floats(rows):
        flat = [float(x) for r in rows for x in r]
        return struct.pack("<%df" % len(flat), *flat)

    pa = acc(push(floats(pos), 34962), 5126, len(pos), "VEC3")
    na = acc(push(floats(nrm), 34962), 5126, len(nrm), "VEC3")
    ua = acc(push(floats(uv), 34962), 5126, len(uv), "VEC2")
    flat_j = [v for j in jnt for v in j]
    ja = acc(push(struct.pack("<%dH" % len(flat_j), *flat_j), 34962), 5123, len(jnt), "VEC4")
    wa = acc(push(floats(wgt)), 5126, len(wgt), "VEC4")
    ia = acc(push(struct.pack("<%dH" % len(idx), *idx), 34963), 5123, len(idx), "SCALAR")
    ibm = [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1] * 2
    ba = acc(push(struct.pack("<32f", *[float(v) for v in ibm])), 5126, 2, "MAT4")
    ta = acc(push(struct.pack("<2f", 0.0, 1.0)), 5126, 2, "SCALAR")
    oa = acc(push(floats([(0, 0, 0), (0, 1, 0)]), 3), 5126, 2, "VEC3")
    img_view = push(png)

    doc = {
        "asset": {"version": "2.0"},
        "accessors": accessors,
        "bufferViews": views,
        "buffers": [{"byteLength": len(blob)}],
        "images": [{"bufferView": img_view, "mimeType": "image/png"}],
        "textures": [{"source": 0}],
        "materials": [{"name": "m", "pbrMetallicRoughness": {"baseColorTexture": {"index": 0}}}],
        "meshes": [{"primitives": [{
            "attributes": {"POSITION": pa, "NORMAL": na, "TEXCOORD_0": ua, "JOINTS_0": ja, "WEIGHTS_0": wa},
            "indices": ia, "material": 0}]}],
        "nodes": [
            {"name": "meshnode", "mesh": 0, "skin": 0},
            {"name": "j0"},
            {"name": "j1", "translation": [4.0, 0.0, 0.0]},
        ],
        "skins": [{"joints": [1, 2], "inverseBindMatrices": ba}],
        "animations": [{
            "name": "lift",
            "samplers": [{"input": ta, "output": oa, "interpolation": "LINEAR"}],
            "channels": [{"sampler": 0, "target": {"node": 2, "path": "translation"}}],
        }],
        "scenes": [{"nodes": [0]}],
        "scene": 0,
    }
    raw = json.dumps(doc).encode()
    while len(raw) % 4:
        raw += b" "
    total = 12 + 8 + len(raw) + 8 + len(blob)
    out = bytearray(b"glTF" + struct.pack("<II", 2, total))
    out.extend(struct.pack("<I", len(raw)) + struct.pack("<I", 0x4E4F534A) + raw)
    out.extend(struct.pack("<I", len(blob)) + struct.pack("<I", 0x004E4942) + bytes(blob))
    return bytes(out)


def write_fixture(tmp: Path) -> Path:
    src = tmp / "grid.glb"
    src.write_bytes(build_grid_glb())
    return src


def run_cli(*argv):
    return subprocess.run(
        [sys.executable, str(REPO / "tools" / "simplify_model.py"), *argv],
        cwd=REPO, capture_output=True, text=True, check=False,
    )


class EmitGlbTests(unittest.TestCase):
    def test_round_trip_preserves_skin_and_animation(self):
        import tempfile

        with tempfile.TemporaryDirectory() as tmp:
            src = write_fixture(Path(tmp))
            parsed = gltf.parse_glb(src)
            model = model_mod.from_gltf(parsed, "grid")
            out = simp_mod.simplify(model, options=simp_mod.SimplificationOptions(target_triangles=10))
            blob = emit_glb_mod.emit_glb(model, out)
            back = gltf.parse_glb_bytes(blob)
            model2 = model_mod.from_gltf(back, "grid2")
            self.assertEqual(len(model2.triangles), len(out.triangles))
            self.assertEqual(len(model2.skins[0].joints), 2)
            self.assertEqual(len(model2.clips), 1)
            self.assertEqual(model2.clips[0].name, "lift")
            self.assertEqual(len(model2.textures), 1)
            self.assertEqual((model2.textures[0].width, model2.textures[0].height), (4, 4))
            # Subset positions are exact.
            for ni, si in enumerate(out.source_vertex):
                self.assertEqual(tuple(out.positions[ni]), tuple(model.vertices[si]))
            # Baked poses agree at kept vertices.
            from model_pipeline.animation import bake_clip_poses

            p0 = bake_clip_poses(model, model.clips[0], [0.5])[0]
            p1 = bake_clip_poses(model2, model2.clips[0], [0.5])[0]
            for ni, si in enumerate(out.source_vertex):
                for a, b in zip(p0[si], p1[ni]):
                    self.assertAlmostEqual(a, b, places=4)

    def test_emit_deterministic_bytes(self):
        import tempfile

        with tempfile.TemporaryDirectory() as tmp:
            src = write_fixture(Path(tmp))
            model = model_mod.from_gltf(gltf.parse_glb(src), "grid")
            out = simp_mod.simplify(model, options=simp_mod.SimplificationOptions(target_triangles=10))
            self.assertEqual(
                emit_glb_mod.emit_glb(model, out), emit_glb_mod.emit_glb(model, out)
            )


class SimplifyCliTests(unittest.TestCase):
    def test_basic_cli_flow(self):
        import tempfile

        with tempfile.TemporaryDirectory() as tmp:
            tmp_path = Path(tmp)
            src = write_fixture(tmp_path)
            dst = tmp_path / "out.glb"
            rep = tmp_path / "out.json"
            r = run_cli("--input", str(src), "--output", str(dst),
                        "--target-triangles", "10", "--report", str(rep))
            self.assertEqual(r.returncode, 0, msg=r.stderr)
            self.assertTrue(dst.exists())
            report = json.loads(rep.read_text())
            for key in ("source", "simplification", "animation_quality", "result"):
                self.assertIn(key, report)
            self.assertEqual(report["source"]["triangles"], 32)
            self.assertLessEqual(report["simplification"]["delivered_triangles"], 32)
            self.assertTrue(report["result"]["pass"])
            self.assertIn("glb_sha256", report["result"])

    def test_cli_deterministic(self):
        import tempfile

        with tempfile.TemporaryDirectory() as tmp:
            tmp_path = Path(tmp)
            src = write_fixture(tmp_path)
            first = tmp_path / "a.glb"
            second = tmp_path / "b.glb"
            for dst in (first, second):
                r = run_cli("--input", str(src), "--output", str(dst), "--target-triangles", "10")
                self.assertEqual(r.returncode, 0, msg=r.stderr)
            self.assertEqual(first.read_bytes(), second.read_bytes())

    def test_cli_upward_search_reported(self):
        import tempfile

        with tempfile.TemporaryDirectory() as tmp:
            tmp_path = Path(tmp)
            src = write_fixture(tmp_path)
            dst = tmp_path / "out.glb"
            # Absurdly tight error cap forces upward search past tiny targets.
            r = run_cli("--input", str(src), "--output", str(dst),
                        "--target-triangles", "2", "--target-error", "0.0000001")
            self.assertEqual(r.returncode, 0, msg=r.stderr)
            self.assertIn("delivered", (r.stdout + r.stderr).lower())

    def test_cli_saturn_cap_hard_failure(self):
        import tempfile

        with tempfile.TemporaryDirectory() as tmp:
            tmp_path = Path(tmp)
            src = write_fixture(tmp_path)
            dst = tmp_path / "out.glb"
            # Quality search delivers several triangles; a 1-triangle cap
            # cannot hold them, so the tool must FAIL loudly, not force it.
            r = run_cli("--input", str(src), "--output", str(dst),
                        "--target-triangles", "10", "--max-triangles", "1")
            self.assertNotEqual(r.returncode, 0)
            self.assertIn("FAIL", r.stdout + r.stderr)

    def test_cli_bad_animation_selector(self):
        import tempfile

        with tempfile.TemporaryDirectory() as tmp:
            src = write_fixture(Path(tmp))
            r = run_cli("--input", str(src), "--output", str(Path(tmp) / "o.glb"),
                        "--animation", "nope")
            self.assertNotEqual(r.returncode, 0)
            self.assertIn("nope", r.stdout + r.stderr)

    def test_cli_generate_lods(self):
        import tempfile

        with tempfile.TemporaryDirectory() as tmp:
            tmp_path = Path(tmp)
            src = write_fixture(tmp_path)
            dst = tmp_path / "chara.glb"
            r = run_cli("--input", str(src), "--output", str(dst),
                        "--target-triangles", "16", "--generate-lods",
                        "--report", str(tmp_path / "r.json"))
            self.assertEqual(r.returncode, 0, msg=r.stderr)
            report = json.loads((tmp_path / "r.json").read_text())
            self.assertTrue(report["lods"])
            for lod in report["lods"]:
                self.assertIn("passed", lod)
                self.assertTrue((tmp_path / Path(lod["output"]).name).exists())

    def test_cli_saturn_profile_pass(self):
        import tempfile

        with tempfile.TemporaryDirectory() as tmp:
            tmp_path = Path(tmp)
            src = write_fixture(tmp_path)
            r = run_cli("--input", str(src), "--output", str(tmp_path / "o.glb"),
                        "--target-triangles", "10", "--profile", "saturn-vdp1",
                        "--report", str(tmp_path / "r.json"))
            self.assertEqual(r.returncode, 0, msg=r.stderr)
            report = json.loads((tmp_path / "r.json").read_text())
            self.assertTrue(report["saturn"]["passed"])

    @unittest.skipUnless(
        (REPO / "examples/basic_3d_animation/assets/male_basic_walk_30_frames_loop.glb").exists(),
        "acceptance GLB not present",
    )
    def test_acceptance_glb_end_to_end(self):
        import tempfile

        with tempfile.TemporaryDirectory() as tmp:
            tmp_path = Path(tmp)
            # Sampled-down to keep the test suite fast; the full-rate
            # acceptance run happens in the Phase 29 pipeline step.
            r = run_cli(
                "--input", "examples/basic_3d_animation/assets/male_basic_walk_30_frames_loop.glb",
                "--output", str(tmp_path / "walk.glb"),
                "--target-triangles", "300", "--quality", "balanced",
                "--animation-aware", "--preserve-silhouette",
                "--sample-fps", "8", "--silhouette-views", "8",
                "--report", str(tmp_path / "walk.json"),
            )
            self.assertEqual(r.returncode, 0, msg=r.stderr[-3000:])
            report = json.loads((tmp_path / "walk.json").read_text())
            self.assertEqual(report["source"]["triangles"], 1552)
            self.assertLessEqual(report["simplification"]["delivered_triangles"], 485)
            back = gltf.parse_glb(tmp_path / "walk.glb")
            self.assertEqual(len(back.json["animations"]), 1)


if __name__ == "__main__":
    unittest.main()
