r"""Acceptance for VDP2 raster effects (examples/vdp2_effects_demo).

    .\harness\run-vdp2-effects.ps1 -Bios .\bios\saturn_bios_us.bin

NBG0 line scroll (sine wave, amplitude 12 dots, period 56 lines), NBG1
vertical cell scroll (per 8-dot column, amplitude 6), back screen gradient.
Judged on the screenshot: the VDP2 does this per line, so the picture is the
proof. The struct read is g_effects_demo.
"""

import os
import re
import struct
import unittest

MAGIC = 0x45464631  # "EFF1"
WRAM_HIGH = 0x06000000
YELLOW = (248, 224, 24)
RED = (248, 48, 48)
BAR_PERIOD = 40      # bars every 5 cells
STRIPE_PERIOD = 32   # stripes every 4 cells
FIRST_ROW = 40       # below the text overlay


def _load():
    dump = os.environ.get("LIBSATURN_EFFECTS_DUMP")
    map_path = os.environ.get("LIBSATURN_EFFECTS_MAP")
    if not dump or not map_path:
        raise unittest.SkipTest("run harness/run-vdp2-effects.ps1 first")
    if not os.path.exists(dump) or not os.path.exists(map_path):
        raise AssertionError("missing input: %s / %s" % (dump, map_path))
    with open(map_path, "r", encoding="utf-8", errors="replace") as stream:
        found = re.search(r"(?m)^\s*0x([0-9A-Fa-f]+)\s+_g_effects_demo\s*$", stream.read())
    if not found:
        raise AssertionError("_g_effects_demo is absent from " + map_path)
    offset = int(found.group(1), 16) - WRAM_HIGH
    with open(dump, "rb") as stream:
        raw = struct.unpack(">9I", stream.read()[offset:offset + 36])
    return dict(zip(("magic", "layers_ok", "line_scroll_ok", "vcs_ok", "back_ok",
                     "refused_bad_layer", "refused_unsupported", "table_bytes", "frames"), raw))


def _near(a, b, tol=24):
    return all(abs(x - y) <= tol for x, y in zip(a, b))


class Vdp2EffectsTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.r = _load()
        if cls.r["magic"] != MAGIC:
            raise AssertionError("g_effects_demo not initialised: %r" % cls.r)
        shots = os.environ.get("LIBSATURN_EFFECTS_SHOTS", "")
        from PIL import Image
        cls.img = Image.open(os.path.join(shots, "e.png")).convert("RGB")
        cls.w, cls.h = cls.img.size
        cls.sx = cls.w / 320.0
        cls.sy = cls.h / 224.0

    def px(self, x, y):
        return self.img.getpixel((min(self.w - 1, int(x * self.sx)), min(self.h - 1, int(y * self.sy))))

    def test_calls_succeeded(self):
        r = self.r
        for key in ("layers_ok", "line_scroll_ok", "vcs_ok", "back_ok",
                    "refused_bad_layer", "refused_unsupported"):
            self.assertEqual(r[key], 1, key)
        self.assertEqual(r["table_bytes"], 224 * 4)

    def first_bar_x(self, y):
        for x in range(320):
            if _near(self.px(x, y), YELLOW):
                return x % BAR_PERIOD
        return None

    def test_line_scroll_waves_every_line_with_the_period(self):
        rows = list(range(FIRST_ROW, 224))
        pos = {y: self.first_bar_x(y) for y in rows}
        self.assertTrue(all(v is not None for v in pos.values()))
        # circular spread over the bar period reflects amplitude 12 (24 peak to peak)
        vals = sorted(set(pos.values()))
        self.assertGreaterEqual(len(vals), 12)
        # the wave repeats every 56 lines and is different half a period away
        same = sum(1 for y in rows if y + 56 < 224 and abs(pos[y] - pos[y + 56]) <= 1)
        total = sum(1 for y in rows if y + 56 < 224)
        self.assertGreater(same, 0.9 * total)
        half = sum(1 for y in rows if y + 28 < 224 and abs(pos[y] - pos[y + 28]) >= 4)
        self.assertGreater(half, 0.5 * sum(1 for y in rows if y + 28 < 224))
        # and it covers the whole screen, not only the first half
        self.assertGreater(len(set(pos[y] for y in range(150, 224))), 8)

    def test_vertical_cell_scroll_ripples_the_columns(self):
        firsts = []
        for col in range(40):
            found = None
            for y in range(FIRST_ROW, 224):
                for dx in range(8):
                    if _near(self.px(col * 8 + dx, y), RED):
                        found = y % STRIPE_PERIOD
                        break
                if found is not None:
                    break
            firsts.append(found)
        self.assertTrue(all(v is not None for v in firsts))
        self.assertGreaterEqual(max(firsts) - min(firsts), 8)

    def test_back_screen_is_a_vertical_gradient(self):
        blues = []
        for y in range(FIRST_ROW, 224, 4):
            best = None
            for x in range(0, 320, 3):
                p = self.px(x, y)
                if p[2] > p[0] + 40 and p[2] > p[1] and not _near(p, YELLOW) and not _near(p, RED):
                    best = p[2]
                    break
            self.assertIsNotNone(best, "no back screen visible at line %d" % y)
            blues.append(best)
        self.assertLessEqual(max(blues[i] - blues[i + 1] for i in range(len(blues) - 1)), 4)
        self.assertGreater(blues[-1] - blues[0], 40)


if __name__ == "__main__":
    unittest.main()
