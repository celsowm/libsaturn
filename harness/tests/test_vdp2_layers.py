r"""Acceptance for the VDP2 layer manager (examples/vdp2_layers_demo).

Run through the wrapper, which builds the demo, runs Ymir, and dumps Work RAM
and screenshots:

    .\harness\run-vdp2-layers.ps1 -Bios .\bios\saturn_bios_us.bin

Four scroll screens (NBG0-NBG3) share the VRAM banks; the library plans the
cycle pattern. The struct read here is g_layers_demo.
"""

import os
import re
import struct
import unittest

MAGIC = 0x4C594531  # "LYE1"
WRAM_HIGH = 0x06000000
# Names in A0 (NBG0, NBG1) and A1 (NBG2, NBG3), characters in B0 and B1: each
# layer reads its names at T0 or T1 and its characters at the same timing in
# the other bank, timings 2 free (no access) and 3.. left to the CPU.
EXPECTED_CYCLE = [0x01FE, 0xEEEE, 0x23FE, 0xEEEE, 0x45FE, 0xEEEE, 0x67FE, 0xEEEE]

YELLOW = (248, 224, 24)   # NBG0 bars, in front
RED = (248, 48, 48)       # NBG1 bars
GREEN = (48, 224, 64)     # NBG2 checker
BLUE = (24, 48, 160)      # NBG3 backdrop grid
MAGENTA = (248, 0, 248)   # sprite selector 1, priority 6
CYAN = (0, 248, 248)      # sprite selector 2, priority 2
WHITE = (248, 248, 248)   # the front square's top-left texel quarter
FRONT_RECT = (128, 96, 64, 48)   # sat_draw_sprite_screen takes the centre
BACK_RECT = (208, 96, 64, 48)


def _load():
    dump = os.environ.get("LIBSATURN_LAYERS_DUMP")
    map_path = os.environ.get("LIBSATURN_LAYERS_MAP")
    if not dump or not map_path:
        raise unittest.SkipTest("run harness/run-vdp2-layers.ps1 first")
    if not os.path.exists(dump) or not os.path.exists(map_path):
        raise AssertionError("missing input: %s / %s" % (dump, map_path))
    with open(map_path, "r", encoding="utf-8", errors="replace") as stream:
        found = re.search(r"(?m)^\s*0x([0-9A-Fa-f]+)\s+_g_layers_demo\s*$", stream.read())
    if not found:
        raise AssertionError("_g_layers_demo is absent from " + map_path)
    offset = int(found.group(1), 16) - WRAM_HIGH
    with open(dump, "rb") as stream:
        image = stream.read()[offset:offset + 88]
    raw = struct.unpack(">22I", image)
    return {
        "image": image,
        "magic": raw[0],
        "configure": [struct.unpack(">i", struct.pack(">I", v))[0] for v in raw[1:5]],
        "cycle": list(raw[5:13]),
        "refused_too_much": raw[13],
        "refused_busy": raw[14],
        "frames": raw[15],
        "palette_priorities": raw[20],
        "color_calc_busy": raw[21],
    }


def _colours(path):
    from PIL import Image
    counts = {}
    for pixel in Image.open(path).convert("RGB").getdata():
        counts[pixel] = counts.get(pixel, 0) + 1
    return counts


class Vdp2LayersTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.r = _load()
        if cls.r["magic"] != MAGIC:
            raise AssertionError("g_layers_demo not initialised: %r" % cls.r)
        cls.shots = os.environ.get("LIBSATURN_LAYERS_SHOTS", "")

    def test_every_layer_was_accepted(self):
        self.assertEqual(self.r["configure"], [0, 0, 0, 0])
        self.assertGreater(self.r["frames"], 150)

    def test_the_allocator_planned_the_expected_cycle_pattern(self):
        self.assertEqual(self.r["cycle"], EXPECTED_CYCLE,
                         " ".join("%04X" % v for v in self.r["cycle"]))

    def test_a_layer_that_cannot_be_fetched_is_refused_and_changes_nothing(self):
        self.assertEqual(self.r["refused_too_much"], 1)

    def test_the_old_nbg0_api_is_refused_while_layers_are_managed(self):
        self.assertEqual(self.r["refused_busy"], 1)

    def test_all_four_layers_are_on_screen(self):
        try:
            import PIL  # noqa: F401
        except ImportError:
            self.skipTest("Pillow is not installed")
        path = os.path.join(self.shots, "f120.png")
        self.assertTrue(os.path.exists(path), path)
        counts = _colours(path)
        for name, colour in (("NBG0 yellow", YELLOW), ("NBG1 red", RED),
                             ("NBG2 green", GREEN), ("NBG3 blue", BLUE)):
            self.assertGreater(counts.get(colour, 0), 1500, "%s missing: %r" % (name, colour))

    def test_nbg2_set_mid_frame_keeps_its_position(self):
        """The demo sets NBG2's scroll to (4, 0) at scanline ~101 and back to (0, 0) at ~140 every
        frame. A mid-frame scroll write applies from that scanline (a SCYN2 write even restarts
        NBG2's line counter), which drew the band shifted; the library now writes NBG scroll only
        in VBlank. Every green pixel must sit on the checker that scroll (0, 0) puts there: cell
        parity (X/8 + Y/8) odd, inner 4x4 parity odd."""
        try:
            from PIL import Image
        except ImportError:
            self.skipTest("Pillow is not installed")
        img = Image.open(os.path.join(self.shots, "f120.png")).convert("RGB")
        px = img.load()
        green, wrong = 0, 0
        for y in range(img.size[1]):
            for x in range(img.size[0]):
                if px[x, y] != GREEN:
                    continue
                green += 1
                cell = ((x >> 3) + (y >> 3)) & 1
                inner = (((x & 7) >> 2) + ((y & 7) >> 2)) & 1
                if not (cell and inner):
                    wrong += 1
        self.assertGreater(green, 1500)
        self.assertEqual(wrong, 0, "%d of %d NBG2 pixels off the (0, 0) checker" % (wrong, green))

    def test_palette_priorities_were_accepted_and_lock_out_color_calc(self):
        self.assertEqual(self.r["palette_priorities"], 0)
        self.assertEqual(self.r["color_calc_busy"], 1)

    def test_each_sprite_is_drawn_at_its_own_priority(self):
        """Sprite type 1: the magenta square (priority 6) covers NBG0's yellow bars, the cyan
        square (priority 2) sits behind them, in the same frame."""
        try:
            from PIL import Image
        except ImportError:
            self.skipTest("Pillow is not installed")
        for shot in ("f60.png", "f120.png"):
            img = Image.open(os.path.join(self.shots, shot)).convert("RGB")
            px = img.load()

            def count(rect):
                x0, y0, w, h = rect
                c = {}
                for y in range(y0, y0 + h):
                    for x in range(x0, x0 + w):
                        c[px[x, y]] = c.get(px[x, y], 0) + 1
                return c
            front, back = count(FRONT_RECT), count(BACK_RECT)
            self.assertEqual(front.get(MAGENTA, 0), 64 * 48 * 3 // 4, "%s front square: %r" % (shot, front))
            # Drawn with FLIP_H | FLIP_V: the texture's top-left quarter lands bottom-right.
            x0, y0, w, h = FRONT_RECT
            self.assertEqual(count((x0 + w // 2, y0 + h // 2, w // 2, h // 2)).get(WHITE, 0), w * h // 4,
                             "%s: flipped quarter not bottom-right" % shot)
            self.assertGreater(back.get(CYAN, 0), 64 * 48 // 2, "%s back square: %r" % (shot, back))
            self.assertGreater(back.get(YELLOW, 0), 0, "%s: NBG0 not in front of the back square" % shot)

    def test_layers_scroll_between_frames(self):
        try:
            from PIL import Image, ImageChops
        except ImportError:
            self.skipTest("Pillow is not installed")
        a = Image.open(os.path.join(self.shots, "f60.png")).convert("RGB")
        b = Image.open(os.path.join(self.shots, "f120.png")).convert("RGB")
        self.assertIsNotNone(ImageChops.difference(a, b).getbbox())

    def test_probe_dump_mem_reads_the_struct(self):
        with open(os.path.join(self.shots, "struct.bin"), "rb") as stream:
            self.assertEqual(stream.read(), self.r["image"])

    def test_no_misaligned_data_access(self):
        """A misaligned word or long access is a CPU address error on the SH-2, but Ymir aligns it
        down and goes on. The probe counts them (--misaligned-report); the library, the BIOS boot
        and this demo must make none."""
        with open(os.path.join(self.shots, "misaligned.txt"), "r", encoding="utf-8") as stream:
            lines = stream.read().splitlines()
        self.assertTrue(lines and lines[0].endswith("(first access per PC; totals: master 0, slave 0)"),
                        "\n".join(lines[:20]))

    def test_probe_screenshot_when_fires_only_once_the_counter_is_reached(self):
        # The wrapper asks for a shot 2 frames after g_layers_demo.frames reaches 100, and one
        # at 0xFFFFFFFF, which the counter never reaches.
        self.assertTrue(os.path.exists(os.path.join(self.shots, "when100.png")))
        self.assertFalse(os.path.exists(os.path.join(self.shots, "never.png")))


if __name__ == "__main__":
    unittest.main()
