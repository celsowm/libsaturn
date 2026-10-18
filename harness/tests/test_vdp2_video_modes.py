r"""Acceptance for VDP2 video modes (E09).

    .\harness\run-vdp2-video-modes.ps1 -Bios .\bios\saturn_bios_us.bin
    .\harness\run-vdp2-video-modes.ps1 -Bios .\bios\saturn_bios_eu.bin -Pal

The example shows the same NBG0 cell pattern (one-dot grid, colour bars, a
one-dot checker, a diagonal) and VDP1 border in every mode this library
supports. A mode change is a full sat_shutdown/sat_init, so each capture is
the result of a real TVMD/VRESO/LSMD write, not a scaled image.

What this test proves:
  - the mode matrix is exact: NTSC has the five 224/240-line modes, PAL has
    all eight including 704x256 and the 320x512/640x512 interlaced pictures;
  - TVMD's HRESO/VRESO/LSMD bits match the mode the example reports;
  - double-density interlace and impossible sizes are refused by the public
    query, and 256 lines are refused on NTSC;
  - the pattern's plane A words are in VDP2 VRAM (probe --dump-vram);
  - each supported mode produced a non-blank picture at the expected size.

Ymir at the pinned revision renders PAL and single-density interlace, so the
PAL run is emulator evidence, not hardware certification.
"""

import json
import os
import unittest

from harness.scripts import telemetry as tel

MAGIC = 0x564D4431  # "VMD1"
FIELDS = (
    "magic", "init_ok", "requested", "supported_mask", "refused_double",
    "refused_width", "refused_height", "refused_pal", "mode_changes", "mode",
    "frames", "width", "height", "interlace", "pal", "ntsc_timing", "hires",
    "cram_mode", "tvmd", "tvstat", "tvmd_ok", "last_error",
)
# (width, height, interlace), in the example's table order.
MODES = (
    (320, 224, 0),
    (352, 240, 0),
    (640, 240, 0),
    (320, 240, 1),
    (640, 240, 1),
    (704, 256, 0),
    (320, 256, 1),
    (640, 256, 1),
)
NTSC_MASK = 0x1F
PAL_MASK = 0xFF


def _vreso(height):
    return 1 if height == 240 else (2 if height == 256 else 0)


def _hreso(width):
    return {352: 1, 640: 2, 704: 3}.get(width, 0)


def _tvmd_bits(mode):
    width, height, interlace = mode
    return ((2 if interlace else 0) << 6) | (_vreso(height) << 4) | _hreso(width)


class VideoModeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.r = _load()
        if cls.r["magic"] != MAGIC:
            raise unittest.SkipTest("run harness/run-vdp2-video-modes.ps1 first")
        cls.pal_run = os.environ.get("LIBSATURN_VIDEOMODES_PAL") == "1"
        cls.shots = os.environ.get("LIBSATURN_VIDEOMODES_SHOTS", "")
        cls.images = {}
        cls.blank = []
        try:
            from PIL import Image
        except ImportError:
            cls.images = None
            return
        for index in range(len(MODES)):
            path = os.path.join(cls.shots, "m%d.png" % index)
            if not os.path.isfile(path):
                continue
            image = Image.open(path).convert("RGB")
            cls.images[index] = image
            colors = image.getcolors(8)
            if colors is None or len(colors) < 3:
                cls.blank.append(index)

    def test_refusal_matrix(self):
        r = self.r
        self.assertEqual(r["init_ok"], 1)
        self.assertEqual(r["requested"], len(MODES))
        self.assertEqual(r["refused_double"], 1)
        self.assertEqual(r["refused_width"], 1)
        self.assertEqual(r["refused_height"], 1)
        self.assertEqual(r["refused_pal"], 1)
        self.assertEqual(r["last_error"], 0)
        self.assertEqual(r["supported_mask"], PAL_MASK if r["pal"] else NTSC_MASK)

    def test_mode_matrix_and_tvmd(self):
        r = self.r
        self.assertEqual(r["pal"], 1 if self.pal_run else 0)
        self.assertIn(r["mode"], range(len(MODES)))
        width, height, interlace = MODES[r["mode"]]
        self.assertEqual(r["width"], width)
        self.assertEqual(r["height"], height)
        self.assertEqual(r["interlace"], interlace)
        self.assertEqual(r["hires"], 1 if width >= 640 else 0)
        self.assertEqual(r["tvmd"] & 0x00FF, _tvmd_bits(MODES[r["mode"]]))
        self.assertEqual(r["tvmd_ok"], 1)
        self.assertGreaterEqual(r["mode_changes"], 5 if not r["pal"] else 8)
        self.assertGreater(r["frames"], 500)
        self.assertEqual(r["cram_mode"], 1)

    def test_each_supported_mode_reached_its_shape(self):
        r = self.r
        mask = r["supported_mask"]
        for index, (width, height, interlace) in enumerate(MODES):
            if not (mask & (1 << index)):
                continue
            path = os.path.join(self.shots, "m%d.png" % index)
            self.assertTrue(os.path.isfile(path), "missing mode %d capture" % index)
            if self.images is not None:
                image = self.images[index]
                self.assertEqual(
                    image.size,
                    (width, height * (2 if interlace else 1)),
                    "mode %d output size" % index,
                )
                self.assertNotIn(index, self.blank, "mode %d picture is blank" % index)

    def test_unsupported_modes_were_never_captured(self):
        mask = self.r["supported_mask"]
        for index in range(len(MODES)):
            if mask & (1 << index):
                continue
            self.assertFalse(
                os.path.exists(os.path.join(self.shots, "m%d.png" % index)),
                "unsupported mode %d was captured" % index,
            )
        self.assertFalse(os.path.exists(os.path.join(self.shots, "never.png")))

    def test_plane_a_words_reached_vdp2_vram(self):
        doc = self._probe()
        ranges = {item["base_word"]: item["words"] for item in doc["vdp2"]["vram_ranges"]}
        self.assertIn(0, ranges, "probe did not dump plane A")
        plane = ranges[0]
        # Top-left cell (0,0) is the first colour bar (tile 2); (8,8) is the
        # 1-dot checker band (tile 6); (48,8) is the diagonal band (tile 7).
        self.assertEqual(plane[0], 2)
        self.assertEqual(plane[8 * 64 + 8], 6)
        self.assertEqual(plane[8 * 64 + 48], 7)
        self.assertEqual(plane[63 * 64 + 63], 0)

    def test_no_misaligned_accesses(self):
        path = os.path.join(self.shots, "misaligned.txt")
        if not os.path.isfile(path):
            self.skipTest("no misaligned report")
        with open(path, "r", encoding="utf-8") as stream:
            lines = stream.read().splitlines()
        self.assertTrue(lines and lines[0].endswith("(first access per PC; totals: master 0, slave 0)"),
                        "\n".join(lines[:20]))

    def _probe(self):
        path = os.environ.get("LIBSATURN_VIDEOMODES_PROBE")
        if not path or not os.path.isfile(path):
            self.skipTest("no probe.json")
        with open(path, "r", encoding="utf-8") as stream:
            return json.load(stream)


def _load():
    dump = os.environ.get("LIBSATURN_VIDEOMODES_DUMP")
    map_path = os.environ.get("LIBSATURN_VIDEOMODES_MAP")
    if not dump or not map_path:
        raise unittest.SkipTest("run harness/run-vdp2-video-modes.ps1 first")
    raw = tel.load_dump(dump, map_path, "_g_video_modes",
                        ">%dI" % len(FIELDS), 4 * len(FIELDS))
    return dict(zip(FIELDS, raw))


if __name__ == "__main__":
    unittest.main()
