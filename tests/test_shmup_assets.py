#!/usr/bin/env python3
"""Quality gate and SATL packer of tools/shmup_assets.py."""

import json
import struct
import sys
import tempfile
import unittest
import zlib
from pathlib import Path

import numpy as np
from PIL import Image

REPO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(REPO / "tools"))

import shmup_assets as sa

ASSETS = REPO / "examples/bullet_hell_stress/assets"
V3_ZIP = Path.home() / "Downloads/shmup_saturn_assets_v3.zip"


def bgr555(r, g, b):
    return ((b >> 3) << 10) | ((g >> 3) << 5) | (r >> 3)


def make_subset(root: Path, sprites: dict, category="bullets", lut=True) -> None:
    """Writes a schema-1 asset directory. Each sprite is an RGBA array; index 1 is its one colour."""
    palette = np.zeros(256, ">u2")
    palette[1] = bgr555(248, 8, 8)
    for sub in ("vdp1/index8", "vdp1/lut4", "sprites_rgba"):
        (root / sub).mkdir(parents=True)
    (root / "palette_bgr555_be.bin").write_bytes(palette.tobytes())
    records = {}
    for name, rgba in sprites.items():
        h, w = rgba.shape[:2]
        Image.fromarray(rgba, "RGBA").save(root / f"sprites_rgba/{name}.png")
        idx = (rgba[..., 3] > 0).astype(np.uint8)
        (root / f"vdp1/index8/{name}.bin").write_bytes(idx.tobytes())
        entry = {"category": category, "w": w, "h": h, "pivot": {"x": w // 2, "y": h // 2},
                 "collision_suggested_radius": 3, "rgba": f"sprites_rgba/{name}.png",
                 "vdp1_index8": f"vdp1/index8/{name}.bin"}
        if lut and w % 8 == 0:
            (root / f"vdp1/lut4/{name}.bin").write_bytes(sa.lut4_pack(idx))
            lut_words = [0] + [0x8000 | bgr555(248, 8, 8)] * 15
            (root / "vdp1/lut4/red.bin").write_bytes(struct.pack(">16H", *lut_words))
            entry.update(lut4_data=f"vdp1/lut4/{name}.bin", lut4_palette="vdp1/lut4/red.bin")
        records[name] = entry
    (root / "manifest.json").write_text(json.dumps(
        {"schema": 1, "package": "t", "version": "1", "palette": "palette_bgr555_be.bin",
         "sprite_records": records}))


def blob_sprite(w=24, h=24, box=(6, 6, 18, 18)) -> np.ndarray:
    a = np.zeros((h, w, 4), np.uint8)
    x0, y0, x1, y1 = box
    a[y0:y1, x0:x1] = (248, 8, 8, 255)
    return a


def validate(sprites, **kw):
    with tempfile.TemporaryDirectory() as tmp:
        make_subset(Path(tmp), sprites, **kw)
        records, meta = sa.load_subset(Path(tmp))
        return sa.validate_records(records, meta)


def kinds(issues):
    return {i.kind for i in issues}


class GateTests(unittest.TestCase):
    def test_clean_sprite_passes(self):
        self.assertEqual(validate({"s": blob_sprite()}), [])

    def test_clipped_edge_is_rejected(self):
        issues = validate({"s": blob_sprite(box=(0, 6, 18, 18))})
        self.assertIn("opaque pixels inside the border (clipped)", kinds(issues))

    def test_one_pixel_gutter_is_not_enough(self):
        issues = validate({"s": blob_sprite(box=(1, 6, 18, 18))})
        self.assertIn("opaque pixels inside the border (clipped)", kinds(issues))

    def test_alpha_halo_is_rejected(self):
        s = blob_sprite()
        s[4, 6:18, :3] = (248, 8, 8)
        s[4, 6:18, 3] = 1
        self.assertIn("non-binary alpha (halo)", kinds(validate({"s": s})))

    def test_neighbour_fragment_is_rejected(self):
        s = blob_sprite(w=40, h=24, box=(10, 6, 30, 18))
        s[10:14, 3:5] = (248, 8, 8, 255)
        issues = validate({"s": s}, lut=False)
        self.assertIn("stray connected component (fragment)", kinds(issues))

    def test_two_shapes_allowed_only_by_name(self):
        s = blob_sprite(w=40, h=24, box=(8, 6, 30, 18))
        s[10:14, 34:36] = (248, 8, 8, 255)
        issues = validate({"pickup_shield_red": s}, category="pickups", lut=False)
        self.assertEqual([str(i) for i in issues], [])
        issues = validate({"pickup_other_red": s}, category="pickups", lut=False)
        self.assertIn("stray connected component (fragment)", kinds(issues))

    def test_width_must_be_multiple_of_eight(self):
        self.assertIn("width not a multiple of 8", kinds(validate({"s": blob_sprite(w=30)}, lut=False)))

    def test_off_centre_pivot_is_rejected(self):
        s = blob_sprite(w=40, h=24, box=(24, 6, 36, 18))
        self.assertIn("pivot off-centre", kinds(validate({"s": s}, lut=False)))

    def test_raw_file_must_match_the_png(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            make_subset(root, {"s": blob_sprite()})
            (root / "vdp1/index8/s.bin").write_bytes(b"\x00" * 10)
            records, meta = sa.load_subset(root)
            self.assertIn("INDEX8 size", kinds(sa.validate_records(records, meta)))

    def test_transparency_mismatch_is_rejected(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            make_subset(root, {"s": blob_sprite()})
            bad = np.zeros((24, 24), np.uint8)
            bad[0:12, 0:12] = 1
            (root / "vdp1/index8/s.bin").write_bytes(bad.tobytes())
            records, meta = sa.load_subset(root)
            self.assertIn("INDEX8 transparency differs from PNG",
                          kinds(sa.validate_records(records, meta)))

    def test_reserved_index_is_rejected(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            make_subset(root, {"s": blob_sprite()})
            raw = bytearray((root / "vdp1/index8/s.bin").read_bytes())
            raw[24 * 12 + 12] = 254
            (root / "vdp1/index8/s.bin").write_bytes(bytes(raw))
            records, meta = sa.load_subset(root)
            self.assertIn("INDEX8 uses reserved index", kinds(sa.validate_records(records, meta)))

    def test_quantisation_over_budget_is_reported(self):
        s = blob_sprite()
        s[6:18, 6:18, :3] = (8, 8, 248)   # blue art, red palette
        self.assertIn("INDEX8 quantisation error over budget", kinds(validate({"s": s}, lut=False)))

    def test_lut4_table_needs_rgb_bit(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            make_subset(root, {"s": blob_sprite()})
            (root / "vdp1/lut4/red.bin").write_bytes(struct.pack(">16H", *([0] + [1] * 15)))
            records, meta = sa.load_subset(root)
            self.assertIn("LUT4 table", kinds(sa.validate_records(records, meta)))


class SupplierPackTests(unittest.TestCase):
    def test_committed_v4_subset_passes(self):
        records, meta = sa.load_subset(ASSETS)
        self.assertEqual(len(records), 146)
        self.assertEqual([str(i) for i in sa.validate_records(records, meta)], [])

    @unittest.skipUnless(V3_ZIP.exists(), "v3 pack not downloaded")
    def test_v3_pack_is_rejected_with_its_real_defects(self):
        root, tmp = sa.open_pack(V3_ZIP)
        try:
            records, meta = sa.load_any(root)
            found = kinds(sa.validate_records(records, meta))
        finally:
            if tmp:
                tmp.cleanup()
        self.assertIn("opaque pixels inside the border (clipped)", found)
        self.assertIn("stray connected component (fragment)", found)
        self.assertIn("non-binary alpha (halo)", found)


FRAME_FMT = ">HHhhHHIIHHHHI"


def parse(blob: bytes) -> dict:
    (magic, version, flags, frames, luts, groups, pal, frame_off, lut_off, pal_off, group_off, total, crc,
     orient) = struct.unpack(">4sHHHHHHIIIIIIH", blob[:42])
    return dict(magic=magic, version=version, frames=frames, luts=luts, groups=groups, total=total,
                crc=crc, frame_off=frame_off, lut_off=lut_off, pal_off=pal_off, group_off=group_off,
                orient=orient)


class PackTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.records, cls.meta = sa.load_subset(ASSETS)
        cls.groups = sa.parse_groups(["enemy_*_red=1", "boss_*=2"])

    def pack(self, profile="lut4", orientation="none"):
        return sa.pack_blob(self.records, self.meta, profile, orientation, self.groups)

    def test_header_crc_and_sector_aligned_groups(self):
        blob, rep = self.pack()
        h = parse(blob)
        self.assertEqual((h["magic"], h["version"], h["frames"]), (b"SATL", 1, 146))
        self.assertEqual(h["total"], len(blob))
        self.assertEqual(h["crc"], zlib.crc32(blob[64:]) & 0xFFFFFFFF)
        for g in rep["groups"]:
            self.assertEqual(g["data_off"] % 2048, 0)
            self.assertEqual(g["data_bytes"] % 2048, 0)

    def test_deterministic(self):
        self.assertEqual(self.pack()[0], self.pack()[0])

    def test_trimming_cuts_vdp1_fill_without_losing_pixels(self):
        _, rep = self.pack()
        self.assertLess(rep["vdp1_pixels"], rep["untrimmed_pixels"] * 0.8)
        for f in rep["frame_list"]:
            self.assertEqual(f["w"] % 8, 0, f["name"])

    def test_frame_pixels_round_trip_to_the_source_art(self):
        blob, rep = self.pack()
        h = parse(blob)
        byname = {r.name: r for r in self.records}
        for i, f in enumerate(rep["frame_list"]):
            rec = struct.unpack(FRAME_FMT, blob[h["frame_off"] + 32 * i:h["frame_off"] + 32 * i + 32])
            w, hh, px, py, fmt, lut, off, size = rec[:8]
            self.assertEqual((w, hh), (f["w"], f["h"]))
            raw = blob[off:off + size]
            if fmt == sa.FORMAT_LUT4:
                pix = sa.lut4_unpack(raw, w, hh)
            else:
                pix = np.frombuffer(raw, np.uint8).reshape(hh, w)
            source_opaque = int((sa.read_rgba(byname[f["name"]].rgba)[..., 3] > 0).sum())
            self.assertEqual(int((pix != 0).sum()), source_opaque, f["name"])

    def test_hires_profile_is_index8_only_and_has_no_luts(self):
        blob, _ = self.pack(profile="hires")
        h = parse(blob)
        self.assertEqual(h["luts"], 0)
        for i in range(h["frames"]):
            fmt = struct.unpack(">H", blob[h["frame_off"] + 32 * i + 8:h["frame_off"] + 32 * i + 10])[0]
            self.assertEqual(fmt, sa.FORMAT_INDEX8)

    def test_lut4_profile_shrinks_storage(self):
        _, lut = self.pack("lut4")
        _, idx = self.pack("hires")
        self.assertLess(lut["sum_frame_bytes"], idx["sum_frame_bytes"] * 0.95)

    def test_rotation_swaps_axes_and_keeps_pixels(self):
        _, a = self.pack("hires", "none")
        blob, c = self.pack("hires", "cw")
        self.assertEqual(parse(blob)["orient"], 1)
        by = {f["name"]: f for f in a["frame_list"]}
        for f in c["frame_list"]:
            self.assertEqual(f["w"] % 8, 0)
            self.assertLessEqual(abs(f["h"] - by[f["name"]]["w"]), 8)

    def test_rotate_point_is_the_inverse_pair(self):
        for x, y in ((0, 0), (5, 9), (24, 12)):
            cx, cy = sa.rotate_point(x, y, 24, 16, "cw")
            self.assertEqual(sa.rotate_point(cx, cy, 16, 24, "ccw"), (x, y))

    def test_dedupe_stores_identical_pixels_once(self):
        a = np.zeros((16, 16, 4), np.uint8)
        a[4:12, 4:12] = (248, 8, 8, 255)
        with tempfile.TemporaryDirectory() as tmp:
            make_subset(Path(tmp), {"a": a, "b": a.copy()}, lut=False)
            records, meta = sa.load_subset(Path(tmp))
            _, rep = sa.pack_blob(records, meta, "hires", "none", [])
        self.assertEqual(rep["unique_pixel_bytes"] * 2, rep["sum_frame_bytes"])

    def test_cli_rejects_defective_assets_before_packing(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp) / "assets"
            make_subset(root, {"s": blob_sprite(box=(0, 6, 18, 18))})
            self.assertEqual(sa.main(["pack", str(root), "--out", str(Path(tmp) / "out")]), 1)
            self.assertFalse((Path(tmp) / "out/SHMUP.ATL").exists())


if __name__ == "__main__":
    unittest.main()
