r"""Acceptance for SCU interrupts (examples/irq_demo).

Run through the wrapper, which builds the demo, runs Ymir and dumps Work RAM:

    .\harness\run-irq-demo.ps1 -Bios .\bios\saturn_bios_us.bin

The demo keeps its counters in ``g_irq_demo``; this test reads that struct
from the Work RAM High dump at the address the linker map gives it.
"""

import os
import re
import struct
import unittest

FRAME_HZ = float(os.environ.get("LIBSATURN_FRAME_HZ", "60"))
MAGIC = 0x49525131  # "IRQ1"
FIELDS = (
    "magic active busy_done busy_ms busy_vblanks vblank_in vblank_out "
    "timer0 sprite_end handler_vblank_out handler_timer0 handler_sprite_end"
).split()
BUSY_VBLANKS = 90
WRAM_HIGH = 0x06000000


def _load():
    dump = os.environ.get("LIBSATURN_IRQ_DUMP")
    map_path = os.environ.get("LIBSATURN_IRQ_MAP")
    if not dump or not map_path or not os.path.exists(dump) or not os.path.exists(map_path):
        raise unittest.SkipTest("run harness/run-irq-demo.ps1 first")
    with open(map_path, "r", encoding="utf-8", errors="replace") as stream:
        found = re.search(r"(?m)^\s*0x([0-9A-Fa-f]+)\s+_g_irq_demo\s*$", stream.read())
    if not found:
        raise AssertionError("_g_irq_demo is absent from " + map_path)
    offset = int(found.group(1), 16) - WRAM_HIGH
    with open(dump, "rb") as stream:
        data = stream.read()[offset:offset + 4 * len(FIELDS)]
    return dict(zip(FIELDS, struct.unpack(">%dI" % len(FIELDS), data)))


class IrqDemoTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.r = _load()
        if cls.r["magic"] != MAGIC:
            raise AssertionError("g_irq_demo not initialised: %r" % cls.r)

    def test_interrupts_are_active(self):
        self.assertEqual(self.r["active"], 1, "sat_init fell back to polling")

    def test_time_counts_through_a_busy_wait(self):
        # 90 VBlank-IN at the console's frame rate (60 Hz NTSC, 50 Hz PAL: run
        # with LIBSATURN_FRAME_HZ=50) with nothing reading the clock. Losing FRT
        # wraps (the bug) reads about 255 ms here.
        self.assertEqual(self.r["busy_done"], 1)
        self.assertEqual(self.r["busy_vblanks"], BUSY_VBLANKS)
        self.assertAlmostEqual(self.r["busy_ms"], BUSY_VBLANKS * 1000 / FRAME_HZ, delta=20)

    def test_every_frame_source_fires_once_per_frame(self):
        frames = self.r["vblank_in"]
        self.assertGreater(frames, 200)
        for name in ("vblank_out", "timer0", "sprite_end"):
            self.assertLessEqual(abs(self.r[name] - frames), 3, name)

    def test_handlers_ran_for_their_sources(self):
        # The handler count trails the source count only by the reads racing
        # a frame boundary.
        for source, handler in (("vblank_out", "handler_vblank_out"),
                                ("timer0", "handler_timer0"),
                                ("sprite_end", "handler_sprite_end")):
            self.assertLessEqual(abs(self.r[source] - self.r[handler]), 2, handler)


if __name__ == "__main__":
    unittest.main()
