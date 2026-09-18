r"""Acceptance assertions for the BIOS-initialized CD Block probe.

Run after:

    .\harness\run-harness.ps1 cd_block_probe -Bios .\bios\saturn_bios_us.bin -Frames 120
    $env:LIBSATURN_PROBE_JSON = 'harness/build/cd_block_probe.json'
    python -m unittest harness.tests.test_cd_block_probe
"""

import json
import os
import unittest


class CDBlockProbeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        path = os.environ.get(
            "LIBSATURN_PROBE_JSON",
            os.path.join(os.path.dirname(__file__), "..", "build", "cd_block_probe.json"),
        )
        if not os.path.exists(path):
            raise unittest.SkipTest(f"CD Block probe JSON not found: {path}")
        with open(path, "r", encoding="utf-8") as handle:
            cls.probe = json.load(handle)
        iso = cls.probe.get("iso_path", "").replace("\\", "/")
        if not iso.endswith("/cd_block_probe.iso"):
            raise unittest.SkipTest(f"probe JSON is for {iso!r}, not cd_block_probe")
        boot = cls.probe.get("boot", {})
        load_addr = boot.get("load_addr", 0)
        pc = boot.get("pc_after_run", 0)
        if not boot.get("injected") or not (load_addr <= pc < load_addr + boot.get("bin_bytes", 0)):
            raise AssertionError("CD Block probe did not execute inside the injected program")

    def test_iso9660_sector_read_reached_success_color(self):
        # SAT_COLOR_GREEN | opaque erase bit. The probe only selects green
        # after reading LBA 16 and checking bytes 1..5 for CD001.
        self.assertEqual(self.probe["vdp1"]["ewdr"], 0x83E0)


if __name__ == "__main__":
    unittest.main()
