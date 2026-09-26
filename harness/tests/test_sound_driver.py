r"""Acceptance for the resident 68000 sound driver (examples/sound_driver_demo) on
Ymir. Run through the wrapper:

    .\harness\run-sound-driver.ps1 -Bios .\bios\saturn_bios_us.bin

The driver runs on the emulated 68000; the checks are its mailbox in Sound RAM,
the log of when each event ran, the tick rate against emulated frames and the
SCSP slot state at the end. The struct read is g_driver_demo.
"""

import json
import os
import re
import struct
import unittest

MAGIC = 0x53445231  # "SDR1"
WRAM_HIGH = 0x06000000
MARKERS = 8
FIELDS = (["magic", "audio_status", "start_status", "running", "base_tick", "marker_status", "play_status",
           "play_start_tick", "key_off_status", "voice_a_slot", "voice_b_slot", "tick_a", "frame_a", "tick_b",
           "frame_b", "heartbeat", "executed", "max_lateness", "queued_at_end"] +
          ["log_tick%d" % i for i in range(MARKERS + 3)] + ["log_late%d" % i for i in range(MARKERS + 3)] +
          ["frames"])
MAILBOX = 0x800
SPACING = 20
NTSC_FRAME_HZ = 59.94
TICK_HZ = 44100.0 / 256.0


def _load():
    base = os.environ.get("LIBSATURN_DRIVER_DIR")
    map_path = os.environ.get("LIBSATURN_DRIVER_MAP")
    if not base or not map_path:
        raise unittest.SkipTest("run harness/run-sound-driver.ps1 first")
    with open(map_path, "r", encoding="utf-8", errors="replace") as stream:
        found = re.search(r"(?m)^\s*0x([0-9A-Fa-f]+)\s+_g_driver_demo\s*$", stream.read())
    if not found:
        raise AssertionError("_g_driver_demo is absent from " + map_path)
    offset = int(found.group(1), 16) - WRAM_HIGH
    with open(os.path.join(base, "wram.bin"), "rb") as stream:
        raw = struct.unpack(">%dI" % len(FIELDS), stream.read()[offset:offset + 4 * len(FIELDS)])
    with open(os.path.join(base, "sram.bin"), "rb") as stream:
        sram = stream.read()
    with open(os.path.join(base, "probe.json"), "r", encoding="utf-8") as stream:
        probe = json.load(stream)
    return dict(zip(FIELDS, raw)), sram, probe


def w16(sram, offset):
    return struct.unpack(">H", sram[offset:offset + 2])[0]


class SoundDriverTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.r, cls.sram, cls.probe = _load()
        if cls.r["magic"] != MAGIC:
            raise AssertionError("g_driver_demo not initialised: %r" % cls.r)

    def test_the_driver_came_up_and_left_its_mailbox(self):
        r = self.r
        self.assertEqual((r["audio_status"], r["start_status"], r["running"]), (0, 0, 1))
        self.assertEqual(struct.unpack(">I", self.sram[MAILBOX:MAILBOX + 4])[0], 0x53445256)   # 'SDRV'
        self.assertEqual(w16(self.sram, MAILBOX + 4), 1)                                       # version
        self.assertEqual(w16(self.sram, MAILBOX + 6) & 1, 1)                                   # running
        self.assertGreater(r["heartbeat"], 100)                                                 # the 68000 kept waking

    def test_every_event_ran_on_its_tick(self):
        r = self.r
        base = r["base_tick"]
        self.assertEqual((r["marker_status"], r["play_status"], r["key_off_status"]), (0, 0, 0))
        due = [base + (i + 1) * SPACING for i in range(MARKERS)]
        due += [r["play_start_tick"]] * 2 + [base + (MARKERS + 2) * SPACING]
        ran = [r["log_tick%d" % i] for i in range(MARKERS + 3)]
        late = [r["log_late%d" % i] for i in range(MARKERS + 3)]
        for i, (want, got, lateness) in enumerate(zip(due, ran, late)):
            self.assertLessEqual(abs(got - (want & 0xFFFF)), 1, "event %d due %d ran at %d" % (i, want, got))
            self.assertLessEqual(lateness, 1, "event %d late by %d" % (i, lateness))
        self.assertEqual(r["executed"], MARKERS + 3)
        self.assertEqual(r["queued_at_end"], 0)
        self.assertLessEqual(r["max_lateness"], 1)

    def test_the_tick_runs_at_the_scsp_timer_rate(self):
        r = self.r
        frames = r["frame_b"] - r["frame_a"]
        ticks = r["tick_b"] - r["tick_a"]
        expected = frames / NTSC_FRAME_HZ * TICK_HZ
        self.assertGreater(frames, 100)
        self.assertLessEqual(abs(ticks - expected), 3, "%d ticks in %d frames, expected %.1f" % (ticks, frames, expected))

    def test_the_timed_voices_reached_the_scsp(self):
        r = self.r
        slots = {s["index"]: s for s in self.probe["scsp"]["slots"]}
        a, b = slots[r["voice_a_slot"]], slots[r["voice_b_slot"]]
        self.assertNotEqual(r["voice_a_slot"], r["voice_b_slot"])
        self.assertTrue(a["key_on"] and a["active"], "voice A was keyed on by the 68000: %r" % a)
        self.assertFalse(b["key_on"], "voice B was keyed off by the 68000 again: %r" % b)


if __name__ == "__main__":
    unittest.main()
