r"""Acceptance assertions for the CD -> CDFS -> VFS music example.

Run after:

    .\harness\run-harness.ps1 cd_streaming_jukebox -Bios .\bios\saturn_bios_us.bin -Frames 300
    $env:LIBSATURN_PROBE_JSON = 'harness/build/probe.json'
    python -m unittest harness.tests.test_cd_streaming_jukebox

The harness automatically enables per-frame SCSP tracing for this example.
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

    def test_scsp_stereo_stream_matches_hardware_contract(self):
        scsp = self.probe["scsp"]
        self.assertEqual(scsp["stream_half_samples"], 4096)
        slots = {slot["index"]: slot for slot in scsp["stream_slots"]}

        left = slots[28]
        right = slots[29]
        for slot in (left, right):
            self.assertTrue(slot["active"])
            self.assertTrue(slot["key_on"])
            self.assertFalse(slot["pcm8"])
            self.assertEqual(slot["loop_control"], 1)  # normal loop
            self.assertEqual(slot["loop_start"], 0)
            self.assertEqual(slot["loop_end"], 8191)
            self.assertLess(slot["curr_sample"], 8192)
            self.assertEqual(slot["octave"], 0xF)  # 22.05 kHz source on a 44.1 kHz SCSP
            self.assertEqual(slot["fns"], 0)
            self.assertEqual(slot["total_level"], 0)
            self.assertEqual(slot["direct_send_level"], 7)
            self.assertTrue(slot["ram_half_hash_valid"])

        self.assertEqual(left["start_address"], 0x70000)
        self.assertEqual(right["start_address"], 0x74000)
        self.assertEqual(left["direct_pan"], 0x1F)
        self.assertEqual(right["direct_pan"], 0x0F)

    def test_refills_modify_only_the_inactive_scsp_half(self):
        scsp = self.probe["scsp"]
        trace = scsp.get("trace", [])
        self.assertGreater(
            len(trace),
            20,
            "jukebox harness must include per-frame SCSP trace data",
        )
        half_samples = scsp["stream_half_samples"]
        refill_events = 0

        for previous_frame, current_frame in zip(trace, trace[1:]):
            previous_slots = {slot["index"]: slot for slot in previous_frame["stream_slots"]}
            current_slots = {slot["index"]: slot for slot in current_frame["stream_slots"]}

            for slot_index in (28, 29):
                previous = previous_slots[slot_index]
                current = current_slots[slot_index]
                if not (
                    previous["active"]
                    and current["active"]
                    and previous["ram_half_hash_valid"]
                    and current["ram_half_hash_valid"]
                ):
                    continue

                changed = [
                    half
                    for half in (0, 1)
                    if previous["ram_half_hashes"][half] != current["ram_half_hashes"][half]
                ]
                if not changed:
                    continue

                refill_events += 1
                self.assertEqual(
                    len(changed),
                    1,
                    f"slot {slot_index} rewrote both SCSP halves between frames "
                    f"{previous_frame['frame']} and {current_frame['frame']}",
                )

                active_half = (current["curr_sample"] // half_samples) & 1
                self.assertNotEqual(
                    changed[0],
                    active_half,
                    f"slot {slot_index} rewrote active half {active_half} at frame "
                    f"{current_frame['frame']} (curr_sample={current['curr_sample']})",
                )

        self.assertGreaterEqual(
            refill_events,
            4,
            "trace did not observe enough steady-state SCSP refills to validate the double buffer",
        )


if __name__ == "__main__":
    unittest.main()
