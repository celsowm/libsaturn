r"""Acceptance assertions for the CD -> CDFS -> VFS music example.

Run after:

    .\harness\run-harness.ps1 cd_streaming_jukebox -Bios .\bios\saturn_bios_us.bin -Frames 300
    $env:LIBSATURN_PROBE_JSON = 'harness/build/cd_streaming_jukebox.json'
    python -m unittest harness.tests.test_cd_streaming_jukebox
"""

import json
import os
import unittest


class CdStreamingJukeboxTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        path = os.environ.get(
            "LIBSATURN_PROBE_JSON",
            os.path.join(os.path.dirname(__file__), "..", "build", "cd_streaming_jukebox.json"),
        )
        if not os.path.exists(path):
            raise unittest.SkipTest(f"CD streaming jukebox JSON not found: {path}")
        with open(path, "r", encoding="utf-8") as handle:
            cls.probe = json.load(handle)
        iso = cls.probe.get("iso_path", "").replace("\\", "/")
        if not iso.endswith("/cd_streaming_jukebox.iso"):
            raise unittest.SkipTest(f"probe JSON is for {iso!r}, not cd_streaming_jukebox")
        boot = cls.probe.get("boot", {})
        load = boot.get("load_addr", 0)
        if not boot.get("injected") or not (load <= boot.get("pc_after_run", 0) < load + boot.get("bin_bytes", 0)):
            raise AssertionError("jukebox did not execute inside the injected program")

    def test_cd_cdfs_vfs_music_path_stays_healthy(self):
        # Green is selected only after CDFS mounting, all three catalog lookups,
        # a non-resident music open/play, and continuous update servicing.
        self.assertEqual(self.probe["vdp1"]["ewdr"], 0x83E0)


if __name__ == "__main__":
    unittest.main()
