"""Synthetic acceptance and independent binary read-back for static HLOD."""

from __future__ import annotations

import math
from pathlib import Path
import json
import subprocess
import struct
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from model_pipeline.gltf import GltfError  # noqa: E402
from model_pipeline.model import SourceModel, SourceTexture  # noqa: E402
from model_pipeline.world_hlod import (  # noqa: E402
    LodTarget,
    build_demo_package,
    build_package,
    demo_source,
    partition_source,
)


HEADER = struct.Struct(">4sHHI32s")
CELL = struct.Struct(">iii6iH")
LEVEL = struct.Struct(">HHIIII")
VERTEX = struct.Struct(">5i")
TRIANGLE = struct.Struct(">4H")


def read_independently(package: bytes) -> dict:
    """Read the public binary layout without using the converter's structs."""
    assert len(package) >= 44
    magic, version, cell_count, declared_bytes, source_hash = struct.unpack_from(
        ">4sHHI32s", package, 0
    )
    assert magic == b"WLH1" and version == 1 and declared_bytes == len(package)
    offset = 44
    cells = []
    for _ in range(cell_count):
        unpacked = struct.unpack_from(">iii6iH", package, offset)
        offset += 38
        key, bounds, lod_count = unpacked[:3], unpacked[3:9], unpacked[9]
        levels = []
        for _ in range(lod_count):
            vcount, fcount, error_fx, texture_bytes, data_offset, data_bytes = struct.unpack_from(
                ">HHIIII", package, offset
            )
            offset += 20
            assert data_bytes == vcount * 20 + fcount * 8
            assert error_fx >= 0 and texture_bytes >= 0
            levels.append({"vertices": vcount, "faces": fcount,
                           "error_fx": error_fx, "texture_bytes": texture_bytes,
                           "offset": data_offset, "bytes": data_bytes})
        cells.append({"key": key, "bounds": bounds, "levels": levels})
    directory_end = offset
    payload_ranges = []
    for cell in cells:
        for level in cell["levels"]:
            start = level["offset"]
            end = start + level["bytes"]
            assert start >= directory_end and end <= len(package)
            payload_ranges.append((start, end))
            material_ids = set()
            largest_vertex_fx = 0
            for vertex_index in range(level["vertices"]):
                values = struct.unpack_from(">5i", package, start + vertex_index * 20)
                largest_vertex_fx = max(largest_vertex_fx, *(abs(v) for v in values))
            face_start = start + level["vertices"] * 20
            indices = []
            for face_index in range(level["faces"]):
                face = struct.unpack_from(">4H", package, face_start + face_index * 8)
                assert max(face[:3]) < level["vertices"]
                indices.extend(face[:3])
                material_ids.add(face[3])
            level["materials"] = material_ids
            level["largest_vertex_fx"] = largest_vertex_fx
            level["indices"] = indices
    assert payload_ranges == sorted(payload_ranges)
    for left, right in zip(payload_ranges, payload_ranges[1:]):
        assert left[1] == right[0]
    assert payload_ranges[-1][1] == len(package)
    return {"source_hash": source_hash.hex(), "cells": cells,
            "directory_end": directory_end}


def simple_model(vertices, uvs=None, triangles=None, materials=None, tri_materials=None,
                 textures=None):
    return SourceModel(
        vertices=vertices,
        uvs=uvs or [(0.0, 0.0)] * len(vertices),
        triangles=triangles or [(0, 1, 2)],
        tri_materials=tri_materials or [0],
        materials=materials if materials is not None else [{}],
        textures=textures or [],
    )


class WorldHlodTests(unittest.TestCase):
    def test_deterministic_package_exact_size_and_independent_readback(self):
        first, report = build_demo_package()
        second, report2 = build_demo_package()
        decoded = read_independently(first)
        self.assertEqual(first, second)
        self.assertEqual(report, report2)
        self.assertEqual(len(first), report["package_bytes"])
        self.assertEqual(decoded["source_hash"], report["source_sha256"])
        self.assertEqual(len(decoded["cells"]), report["cell_count"])
        self.assertAlmostEqual(report["source_area_coverage"], 1.0, places=8)
        self.assertEqual(report["source_triangle_coverage"], 1.0)
        for cell in report["cells"]:
            self.assertEqual(len(cell["source_sha256"]), 64)
            for level in cell["levels"]:
                self.assertTrue(level["budget_passed"])
                self.assertEqual(level["source_sha256"], cell["source_sha256"])
                self.assertEqual(
                    [c["name"] for c in level["fixed_camera_view_error"]["cameras"]],
                    ["front", "oblique", "elevated"],
                )
        self.assertEqual(report["payload_bytes"] + report["directory_bytes"]
                         + report["header_bytes"], len(first))

    def test_uv_seams_and_material_cuts_survive_partition_and_simplification(self):
        # Shared positions have intentionally different UVs at the chart seam.
        model = simple_model(
            vertices=[(0, 0, 0), (1, 0, 0), (0, 1, 0),
                      (1, 0, 0), (1, 1, 0), (0, 1, 0)],
            uvs=[(0, 0), (1, 0), (0, 1), (0, 0), (1, 1), (1, 1)],
            triangles=[(0, 1, 2), (3, 4, 5)],
            materials=[{"name": "left"}, {"name": "right"}],
            tri_materials=[0, 1],
        )
        cell = next(iter(partition_source(model, 8.0).values()))
        pairs = {}
        for p, uv in zip(cell.vertices, cell.uvs):
            pairs.setdefault(p, set()).add(uv)
        self.assertGreaterEqual(sum(len(values) > 1 for values in pairs.values()), 2)
        # Two materials remain distinct after all variants, and fixed-camera
        # quality reports are present for the door/window acceptance model.
        package, report = build_package(
            model, cell_size=8.0,
            lod_targets=(LodTarget("near", 2), LodTarget("far", 2)),
            quality="aggressive", silhouette_views=8,
        )
        readback = read_independently(package)
        self.assertEqual(readback["cells"][0]["levels"][0]["materials"], {0, 1})
        self.assertEqual(readback["cells"][0]["levels"][1]["materials"], {0, 1})
        for level in report["cells"][0]["levels"]:
            self.assertIn("view_error", level)
            self.assertGreaterEqual(level["view_error"]["iou_min"], 0.0)
            self.assertLessEqual(level["view_error"]["iou_min"], 1.0)

    def test_crossing_triangles_are_clipped_without_area_or_winding_loss(self):
        model = simple_model(
            vertices=[(0.1, 0.1, 0.1), (1.8, 0.1, 0.1), (0.1, 1.8, 1.8)],
            triangles=[(0, 1, 2)],
        )
        cells = partition_source(model, 1.0)
        self.assertGreater(len(cells), 1)
        self.assertEqual(set().union(*(set(c.source_triangle_ids) for c in cells.values())), {0})
        source_area = 0.5 * math.sqrt(1.7**2 * 1.7**2 + 1.7**2 * 1.7**2)
        clipped_area = 0.0
        for cell in cells.values():
            for tri in cell.triangles:
                a, b, c = (cell.vertices[i] for i in tri)
                ab = tuple(b[i] - a[i] for i in range(3))
                ac = tuple(c[i] - a[i] for i in range(3))
                cross = (ab[1]*ac[2]-ab[2]*ac[1], ab[2]*ac[0]-ab[0]*ac[2],
                         ab[0]*ac[1]-ab[1]*ac[0])
                clipped_area += 0.5 * math.sqrt(sum(v*v for v in cross))
        self.assertAlmostEqual(clipped_area, source_area, places=8)

    def test_texture_limits_palette_references_and_uv_coordinate_guards(self):
        too_wide = SourceTexture(512, 1, [(0, 0, 0, 255)] * 512)
        model = simple_model([(0, 0, 0), (1, 0, 0), (0, 1, 0)],
                             materials=[{"texture_index": 0}], textures=[too_wide])
        with self.assertRaisesRegex(GltfError, "VDP1 limits"):
            build_package(model, lod_targets=(LodTarget("near", 1),))
        bad_ref = simple_model([(0, 0, 0), (1, 0, 0), (0, 1, 0)],
                               materials=[{"texture_index": 1}],
                               textures=[SourceTexture(8, 1, [(0, 0, 0, 255)] * 8)])
        with self.assertRaisesRegex(GltfError, "invalid texture"):
            build_package(bad_ref, lod_targets=(LodTarget("near", 1),))
        bad_uv = simple_model([(0, 0, 0), (1, 0, 0), (0, 1, 0)],
                              uvs=[(40000, 0), (0, 0), (0, 1)])
        with self.assertRaisesRegex(GltfError, "signed 16.16"):
            build_package(bad_uv, lod_targets=(LodTarget("near", 1),))

    def test_large_logical_world_uses_cell_relative_fixed_point(self):
        model = simple_model([(4096, 0, 0), (4097, 0, 0), (4096, 1, 0)])
        package, report = build_package(model, cell_size=16.0,
                                        lod_targets=(LodTarget("near", 1),))
        readback = read_independently(package)
        self.assertEqual(readback["cells"][0]["key"][0], 256)
        self.assertGreater(report["cells"][0]["cell"][0] * report["cell_size"], 181)
        self.assertLessEqual(max(abs(v) for v in readback["cells"][0]["bounds"]), 16 * 65536)

    def test_invalid_targets_and_empty_or_malformed_sources_fail_closed(self):
        model = simple_model([(0, 0, 0), (1, 0, 0), (0, 1, 0)])
        with self.assertRaisesRegex(GltfError, "non-increasing"):
            build_package(model, lod_targets=(LodTarget("near", 1), LodTarget("far", 2)))
        with self.assertRaisesRegex(GltfError, "material count"):
            build_package(SourceModel(vertices=model.vertices, triangles=model.triangles,
                                       materials=[{}], tri_materials=[]),
                          lod_targets=(LodTarget("near", 1),))
        with self.assertRaisesRegex(GltfError, "no vertices or triangles"):
            build_package(SourceModel(), lod_targets=(LodTarget("near", 1),))

    def test_demo_preserves_door_window_materials_and_quantified_views(self):
        package, report = build_demo_package()
        decoded = read_independently(package)
        materials = set().union(*(level["materials"] for cell in decoded["cells"]
                                  for level in cell["levels"]))
        self.assertTrue({0, 1, 2, 3, 4}.issubset(materials))
        self.assertEqual(report["protected_regions"], ["door-frame", "window-frames"])
        self.assertEqual(len(report["validation_camera_directions"]), 12)
        self.assertEqual(report["largest_vdp1_sprite_extent"], [8, 8])
        lod2 = [level for cell in report["cells"] for level in cell["levels"]
                if level["name"] == "lod2"]
        self.assertTrue(lod2)
        self.assertGreater(min(level["view_error"]["iou_min"] for level in lod2), 0.9)
        self.assertLess(max(level["view_error"]["chamfer_px_max"] for level in lod2), 1.0)
        self.assertGreater(min(level["fixed_camera_view_error"]["iou_min"]
                               for level in lod2), 0.9)
        self.assertLess(max(level["fixed_camera_view_error"]["chamfer_px_max"]
                            for level in lod2), 1.0)

    def test_example_build_generator_records_exact_package_accounting(self):
        with tempfile.TemporaryDirectory(prefix="world-hlod-") as temp:
            out = Path(temp)
            binary, source_c = out / "scene.wlh", out / "scene.c"
            header, report_path = out / "scene.h", out / "report.json"
            result = subprocess.run(
                [sys.executable, str(ROOT / "tools/build_world_hlod_demo.py"),
                 "--output", str(binary), "--source", str(source_c),
                 "--header", str(header),
                 "--report", str(report_path)],
                check=True, capture_output=True, text=True,
            )
            report = json.loads(report_path.read_text(encoding="utf-8"))
            generated_header = header.read_text(encoding="ascii")
            self.assertIn(f"{binary.stat().st_size} exact bytes", result.stdout)
            self.assertEqual(binary.stat().st_size, report["package_bytes"])
            self.assertIn(f"SAT_WORLD_HLOD_DEMO_PACKAGE_BYTES {report['package_bytes']}u",
                          generated_header)
            self.assertIn(f"SAT_WORLD_HLOD_DEMO_CELL_COUNT {report['cell_count']}u",
                          generated_header)
            self.assertIn("MAX_LOCAL_COORD_FX16", generated_header)
            self.assertIn("g_world_hlod_demo_package[]", source_c.read_text(encoding="ascii"))

    def test_source_checksum_is_content_based(self):
        original = demo_source()
        _, original_report = build_package(
            original, cell_size=16.0,
            lod_targets=(LodTarget("near", 1000),), quality="aggressive")
        changed = demo_source()
        changed.vertices[0] = (changed.vertices[0][0] + 0.125,
                               changed.vertices[0][1], changed.vertices[0][2])
        _, changed_report = build_package(
            changed, cell_size=16.0,
            lod_targets=(LodTarget("near", 1000),), quality="aggressive")
        self.assertNotEqual(original_report["source_sha256"], changed_report["source_sha256"])


if __name__ == "__main__":
    unittest.main()
