r"""Acceptance for VDP2 dynamic tile/map streaming (E07).

    .\harness\run-vdp2-dynamic-tile-stream.ps1 -Bios .\bios\saturn_bios_us.bin
    .\harness\run-vdp2-dynamic-tile-stream.ps1 -Bios .\bios\saturn_bios_us.bin -Stress

Two pattern-name rings (0x00000 / 0x04000) hold the same 64x64-cell map.
Each frame the inactive ring is updated through sat_stage_map2 and the map
base + scroll are published for the next VBlank. The active ring is never
written. The test recomputes the CRC of the active resident window from the
probe's VDP2 VRAM dump and matches the example's model/VRAM CRCs. There is no
golden image: the road pixels come from the 5-bit palette and the sentinel
canary proves a missing cell would be magenta.
"""

import json
import os
import struct
import unittest

from harness.scripts import telemetry as tel

MAGIC = 0x54535431  # "TST1"
FIELDS = (
    "magic", "init_ok", "storage_ok", "refused_view", "refused_base",
    "refused_dma", "capacity_errors", "write_errors", "switches", "frames",
    "active", "active_base", "win_x0", "win_y0", "win_cols", "win_rows",
    "crc_expected", "crc_vram", "crc_mismatches", "sentinel_cells",
    "max_pending_words", "staged_words", "committed_words", "dma_scu",
    "dma_cpu", "dma_bytes_scu", "dma_bytes_cpu", "dma_timeouts",
    "dma_illegal", "scroll_y", "frozen", "crc_frame",
)
STRESS = os.environ.get("LIBSATURN_TILESTREAM_STRESS") == "1"
ASPHALT = (128, 128, 144)  # palette (16,16,18) expanded by <<3
GRASS = ((32, 192, 32), (16, 128, 16))  # palette (4,24,4)/(2,16,2)
MAGENTA = (248, 0, 248)   # palette 15, the sentinel canary
CRITICAL_SHOTS = ("early", "mid", "late")


def crc_words(words):
    """The example's CRC-32: XOR each 16-bit word, shift 16 times."""
    crc = 0xFFFFFFFF
    for value in words:
        crc ^= value
        for _ in range(16):
            crc = (crc >> 1) ^ (0xEDB88320 if crc & 1 else 0)
    return crc ^ 0xFFFFFFFF


def _near(a, b, tol=2):
    return all(abs(x - y) <= tol for x, y in zip(a, b))


class DynamicTileStreamTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.r = _load()
        if cls.r["magic"] != MAGIC:
            raise unittest.SkipTest("run harness/run-vdp2-dynamic-tile-stream.ps1 first")
        from PIL import Image
        cls.shots = os.environ.get("LIBSATURN_TILESTREAM_SHOTS", "")
        cls.images = {}
        for name in CRITICAL_SHOTS + (("long",) if STRESS else ()):
            path = os.path.join(cls.shots, name + ".png")
            if not os.path.isfile(path):
                continue
            image = Image.open(path).convert("RGB")
            assert image.size == (320, 224), (name, image.size)
            cls.images[name] = image
        cls.misaligned = os.path.join(cls.shots, "misaligned.txt")

    def assertPixel(self, image, x, y, want, label):
        got = image.getpixel((x, y))
        self.assertTrue(_near(got, want), "%s (%d,%d): %r want %r" % (label, x, y, got, want))

    def test_telemetry_and_refusals(self):
        r = self.r
        for key in ("init_ok", "storage_ok", "refused_view", "refused_base", "refused_dma"):
            self.assertEqual(r[key], 1, key)
        self.assertEqual(r["capacity_errors"], 0)
        self.assertEqual(r["write_errors"], 0)
        self.assertIn(r["active"], (0, 1))
        self.assertEqual(r["win_cols"], 45)
        self.assertEqual(r["win_rows"], 33)
        self.assertEqual(r["win_x0"], 62)
        self.assertLess(r["win_y0"], 64)
        self.assertLess(r["scroll_y"], 512)
        self.assertEqual(r["crc_expected"], r["crc_vram"], "model and VRAM CRC differ")
        self.assertEqual(r["crc_mismatches"], 0)
        self.assertEqual(r["sentinel_cells"], 0, "an uninitialised 0xFFFF cell is visible")
        self.assertLessEqual(r["max_pending_words"], 2970)
        self.assertGreater(r["staged_words"], 0)
        self.assertGreater(r["committed_words"], 0)
        self.assertEqual(r["staged_words"], r["committed_words"])
        self.assertGreater(r["dma_scu"], 0, "no SCU-DMA transfer")
        self.assertGreater(r["dma_cpu"], 0, "no CPU transfer path")
        self.assertGreater(r["dma_bytes_scu"], 0)
        self.assertGreater(r["dma_bytes_cpu"], 0)
        self.assertEqual(r["dma_timeouts"], 0)
        self.assertEqual(r["dma_illegal"], 0)
        if STRESS:
            self.assertEqual(r["frozen"], 1, "stress did not reach the freeze frame")
            self.assertGreater(r["frames"], 32000)
            self.assertGreaterEqual(r["crc_frame"], 33000)
            self.assertGreaterEqual(r["switches"], 32000)
        else:
            self.assertGreater(r["frames"], 2000)
            self.assertGreater(r["crc_frame"], 0)
            self.assertGreaterEqual(r["switches"], r["frames"] - 3)

    def test_base_switch_reaches_the_register(self):
        doc = self._probe()
        regs = doc["vdp2"]
        self.assertIn(regs["mpabn0"], (0x0000, 0x0202))
        self.assertEqual(regs["mpcdn0"], regs["mpabn0"])
        self.assertEqual(regs["mpofn"] & 0x000F, 0, "NBG0 map offset changed")
        if STRESS:
            self.assertEqual(self.r["frozen"], 1)
            want = 0x0202 if self.r["active_base"] == 0x04000 else 0x0000
            self.assertEqual(regs["mpabn0"], want)

    def test_ring_switch_watch_contains_both_bases(self):
        doc = self._probe()
        watch = doc.get("watch_writes")
        if not watch:
            self.skipTest("no --watch-write capture")
        self.assertEqual(watch["address"], 0x05F80040)
        values = {hit["value"] for hit in watch["master"]["hits"]}
        self.assertIn(0x0000, values, "ring A base was never watched")
        self.assertIn(0x0202, values, "ring B base was never watched")
        self.assertGreater(watch["master"]["count"], 0)

    def test_vram_dump_matches_the_frozen_crc(self):
        if not STRESS:
            self.skipTest("the short run's snapshot is not frozen; use -Stress")
        doc = self._probe()
        ranges = {r["base_word"]: r["words"] for r in doc["vdp2"]["vram_ranges"]}
        base = self.r["active_base"] // 2
        self.assertIn(base, ranges, "probe did not dump the active ring")
        ring = ranges[base]
        words = []
        for row in range(self.r["win_rows"]):
            cy = (self.r["win_y0"] + row) & 63
            for col in range(self.r["win_cols"]):
                cx = (self.r["win_x0"] + col) & 63
                words.append(ring[cy * 64 + cx])
        self.assertEqual(crc_words(words), self.r["crc_vram"])
        self.assertEqual(crc_words(words), self.r["crc_expected"])
        self.assertNotIn(0xFFFF, words)

    def test_road_pixels_and_no_sentinel(self):
        for name in CRITICAL_SHOTS:
            self.assertIn(name, self.images, "missing %s capture" % name)
            image = self.images[name]
            # Centre lane is asphalt at every scroll; the left edge is grass.
            self.assertPixel(image, 160, 100, ASPHALT, name + " asphalt")
            got = image.getpixel((8, 100))
            self.assertTrue(any(_near(got, c) for c in GRASS),
                            "%s grass: %r" % (name, got))
            # Any 0xFFFF map cell would show palette 15's magenta canary.
            for y in range(60, 151, 2):
                for x in range(0, 320, 2):
                    self.assertNotEqual(image.getpixel((x, y)), MAGENTA,
                                        "%s has a sentinel cell at (%d,%d)" % (name, x, y))
        self.assertNotEqual(self.images["early"].tobytes(), self.images["late"].tobytes(),
                            "the road did not scroll")
        if STRESS:
            self.assertIn("long", self.images)
            self.assertPixel(self.images["long"], 160, 100, ASPHALT, "frozen asphalt")

    def test_no_misaligned_accesses(self):
        if not self.misaligned or not os.path.isfile(self.misaligned):
            self.skipTest("no misaligned report")
        with open(self.misaligned, "r", encoding="utf-8") as stream:
            lines = stream.read().splitlines()
        self.assertTrue(lines and lines[0].endswith("(first access per PC; totals: master 0, slave 0)"),
                        "\n".join(lines[:20]))

    def test_probe_dump_mem_reads_the_struct(self):
        with open(os.path.join(self.shots, "struct.bin"), "rb") as stream:
            self.assertEqual(stream.read(),
                             struct.pack(">%dI" % len(FIELDS), *[self.r[k] for k in FIELDS]))

    def test_probe_screenshot_when_fires_only_when_reached(self):
        for name in CRITICAL_SHOTS:
            self.assertIn(name, self.images)
        if STRESS:
            self.assertIn("long", self.images)
        self.assertFalse(os.path.exists(os.path.join(self.shots, "never.png")))

    def _probe(self):
        path = os.environ.get("LIBSATURN_TILESTREAM_PROBE")
        if not path or not os.path.isfile(path):
            self.skipTest("no probe.json")
        with open(path, "r", encoding="utf-8") as stream:
            return json.load(stream)


def _load():
    dump = os.environ.get("LIBSATURN_TILESTREAM_DUMP")
    map_path = os.environ.get("LIBSATURN_TILESTREAM_MAP")
    if not dump or not map_path:
        raise unittest.SkipTest("run harness/run-vdp2-dynamic-tile-stream.ps1 first")
    raw = tel.load_dump(dump, map_path, "_g_tile_stream",
                        ">%dI" % len(FIELDS), 4 * len(FIELDS))
    return dict(zip(FIELDS, raw))


if __name__ == "__main__":
    unittest.main()
