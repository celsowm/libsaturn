"""Skybridge 3D hardware smoke probes (GPL-3.0, harness/LICENSE).

Run the example with your own BIOS first:
  .\\harness\\run-harness.ps1 skybridge_3d -Bios <dump> -Frames 300 \\
    -PadScript .\\harness\\scripts\\skybridge_smoke.pad
Then run: python -m unittest discover harness/tests -k skybridge
These register/VRAM assertions do not establish that the scene looks or plays correctly.
"""
import json
import os
import unittest

RP = 0x10000
COEF = 0x12000
HORIZON = 96


def range_words(probe, base, count):
    for region in probe["vdp2"]["vram_ranges"]:
        start = region["base_word"]
        data = region["words"]
        if start <= base and base + count <= start + len(data):
            return data[base - start:base - start + count]
    raise AssertionError(f"requested VRAM range missing: {base:#x}, {count} words")


class SkybridgeProbeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        source = os.environ.get("LIBSATURN_PROBE_JSON")
        if not source:
            source = os.path.join(os.path.dirname(__file__), "..", "build", "probe.json")
        if not os.path.isfile(source):
            raise unittest.SkipTest("Skybridge probe.json not found; run run-harness.ps1 first")
        with open(source, encoding="utf-8") as handle:
            cls.probe = json.load(handle)
        if not cls.probe.get("iso_path", "").replace("\\", "/").endswith("/skybridge_3d.iso"):
            raise unittest.SkipTest("The selected probe.json belongs to a different example")
        boot = cls.probe.get("boot")
        if not boot or not boot.get("injected"):
            raise AssertionError("Skybridge binary was not injected")
        start = boot["load_addr"]
        if not start <= boot["pc_after_run"] < start + boot["bin_bytes"]:
            raise AssertionError("Execution left the injected Skybridge program")

    def test_sky_and_ocean_layers(self):
        video = self.probe["vdp2"]
        self.assertNotEqual(video["bgon"] & (1 << 0), 0, "NBG0 sky disabled")
        self.assertNotEqual(video["bgon"] & (1 << 4), 0, "RBG0 sea disabled")
        self.assertEqual(video["bgon"] & (1 << 12), 0, "RBG0 coefficient transparency disabled")
        self.assertEqual(video["ktctl"] & 1, 1, "RBG0 coefficients disabled")
        self.assertEqual(self.probe["vdp1"]["ewdr"], 0, "Opaque erase hides the backgrounds")

    def test_ocean_parameter_table(self):
        params = range_words(self.probe, RP, 48)
        self.assertNotEqual(params[2], params[27], "RBG0 Yst == Py: vertical depth vanishes")
        self.assertEqual(params[42], (COEF * 2) // 4, "wrong coefficient-table address")
        self.assertEqual(params[44], 1, "coefficient table does not advance per scanline")

    def test_horizon_is_transparent(self):
        coeff = range_words(self.probe, COEF, 448)
        for row in range(224):
            alpha_bit = coeff[row * 2] & 0x8000
            self.assertEqual(bool(alpha_bit), row <= HORIZON,
                             f"unexpected RBG0 transparency on row {row}")


if __name__ == "__main__":
    unittest.main()
