r"""Acceptance for examples/physics3_bench on the SH-2.

Run after:

    .\harness\run-harness.ps1 physics3_bench -Bios .\bios\saturn_bios_us.bin -Frames 2400
    $env:LIBSATURN_PROBE_JSON = 'harness/build/probe.json'
    python -m unittest harness.tests.test_physics3_bench

The example steps seven physics configurations (linear scan, BVH, mesh grid,
mesh CCD, sphere/sphere) and turns the VDP1 erase colour green only when
every sat_physics3_world_step returned SAT_OK. Per-configuration timings live
in g_bench_results in Work RAM; see docs/PHYSICS3_WORLD.md for the numbers.
"""

import json
import os
import unittest


class Physics3BenchTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        path = os.environ.get(
            "LIBSATURN_PROBE_JSON",
            os.path.join(os.path.dirname(__file__), "..", "build", "probe.json"),
        )
        if not os.path.exists(path):
            raise unittest.SkipTest(f"probe JSON not found: {path}")
        with open(path, "r", encoding="utf-8") as handle:
            cls.probe = json.load(handle)
        iso = cls.probe.get("iso_path", "").replace("\\", "/")
        if not iso.endswith("/physics3_bench.iso"):
            raise unittest.SkipTest(f"probe JSON is for {iso!r}, not physics3_bench")
        boot = cls.probe.get("boot", {})
        load = boot.get("load_addr", 0)
        if not boot.get("injected") or not (load <= boot.get("pc_after_run", 0) < load + boot.get("bin_bytes", 0)):
            raise AssertionError("physics3_bench did not execute inside the injected program")

    def test_every_configuration_stepped(self):
        # Green (0x83E0) is set only after all seven configurations finished
        # without an error; red means one failed, black that it never ended.
        self.assertEqual(self.probe["vdp1"]["ewdr"], 0x83E0)


if __name__ == "__main__":
    unittest.main()
