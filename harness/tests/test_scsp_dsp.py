r"""Acceptance for the SCSP DSP effects (examples/scsp_dsp_demo) on Ymir. Run through the wrapper:

    .\harness\run-scsp-dsp.ps1 -Bios .\bios\saturn_bios_us.bin

The demo plays a click through the echo and, later, through the reverb. The probe records the SCSP
output from the program start (audio.raw, stereo int16). The checks are on that output (where each
repeat lands and how loud it is), on the echo's delay line read back by the demo (g_echo_ring) and on
what the API reported (g_dsp_demo).
"""

import array
import os
import re
import struct
import sys
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "tools"))
import scsp_dsp  # noqa: E402

MAGIC = 0x44535031  # "DSP1"
WRAM_HIGH = 0x06000000
RING_WORDS = 8192
FIELDS = ["magic", "audio_status", "echo_status", "echo_click_status", "echo_ring_offset", "echo_ring_bytes",
          "echo_delay_samples", "echo_kind", "snapshot_status", "reverb_status", "reverb_click_status",
          "reverb_kind", "reverb_ring_bytes", "stop_status", "kind_after_stop", "frames"]
ECHO_DELAY = 4410          # 100 ms
CLICK_PEAK = 12000
GAP = 200                  # quiet samples that separate two bursts


def _symbol(map_text, name):
    found = re.search(r"(?m)^\s*0x([0-9A-Fa-f]+)\s+_%s\s*$" % name, map_text)
    if not found:
        raise AssertionError("_%s is absent from the map" % name)
    return int(found.group(1), 16) - WRAM_HIGH


def _load():
    base = os.environ.get("LIBSATURN_DSP_DIR")
    map_path = os.environ.get("LIBSATURN_DSP_MAP")
    if not base or not map_path:
        raise unittest.SkipTest("run harness/run-scsp-dsp.ps1 first")
    with open(map_path, "r", encoding="utf-8", errors="replace") as stream:
        map_text = stream.read()
    with open(os.path.join(base, "wram.bin"), "rb") as stream:
        wram = stream.read()
    offset = _symbol(map_text, "g_dsp_demo")
    results = dict(zip(FIELDS, struct.unpack(">%dI" % len(FIELDS), wram[offset:offset + 4 * len(FIELDS)])))
    ring_at = _symbol(map_text, "g_echo_ring")
    ring = struct.unpack(">%dh" % RING_WORDS, wram[ring_at:ring_at + 2 * RING_WORDS])
    samples = array.array("h")
    with open(os.path.join(base, "audio.raw"), "rb") as stream:
        samples.frombytes(stream.read())
    if sys.byteorder == "big":
        samples.byteswap()
    left = samples[0::2]
    right = samples[1::2]
    return results, ring, left, right


def bursts(values, threshold, gap=GAP, circular=False):
    """(first index, last index, peak) of every run of samples above the threshold, runs closer than
    `gap` samples merged. A circular sequence joins a run that wraps around its end."""
    hits = [i for i, v in enumerate(values) if abs(v) > threshold]
    runs = []
    for i in hits:
        if runs and i - runs[-1][1] <= gap:
            runs[-1][1] = i
            runs[-1][2] = max(runs[-1][2], abs(values[i]))
        else:
            runs.append([i, i, abs(values[i])])
    if circular and len(runs) > 1 and runs[0][0] + len(values) - runs[-1][1] <= gap:
        first = runs.pop(0)
        runs[-1][1] = first[1] + len(values)
        runs[-1][2] = max(runs[-1][2], first[2])
    return [tuple(r) for r in runs]


def centroid(values, run, margin=8):
    """Energy-weighted position of a burst: it does not move when the burst is scaled, unlike the
    first sample above a threshold (the click has an attack ramp)."""
    low = max(0, run[0] - margin)
    high = min(len(values), run[1] + margin + 1)
    energy = sum(values[i] * values[i] for i in range(low, high))
    return sum(i * values[i] * values[i] for i in range(low, high)) / energy


class ScspDspTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.r, cls.ring, cls.left, cls.right = _load()
        if cls.r["magic"] != MAGIC:
            raise AssertionError("g_dsp_demo not initialised: %r" % cls.r)
        cls.found = bursts(cls.left, 60)

    def test_the_api_reported_success(self):
        r = self.r
        self.assertEqual((r["audio_status"], r["echo_status"], r["echo_click_status"], r["snapshot_status"],
                          r["reverb_status"], r["reverb_click_status"], r["stop_status"]), (0,) * 7)
        self.assertEqual((r["echo_kind"], r["reverb_kind"], r["kind_after_stop"]), (1, 2, 0))
        self.assertEqual((r["echo_ring_bytes"], r["echo_delay_samples"]), (2 * RING_WORDS, ECHO_DELAY))
        self.assertEqual(r["echo_ring_offset"] % 0x2000, 0)
        self.assertEqual(r["reverb_ring_bytes"], 0x8000)

    def test_the_output_channels_match(self):
        # pan centre and a mono effect return: the two sides carry the same signal
        peak = max(abs(v) for v in self.left)
        self.assertGreater(peak, 5000, "the click was never heard")
        self.assertLessEqual(max(abs(a - b) for a, b in zip(self.left, self.right)), max(4, peak // 100))

    def _phases(self):
        """The bursts of the echo phase and of the reverb phase, split at the long silence between them."""
        found = self.found
        split = [i for i in range(1, len(found)) if found[i][0] - found[i - 1][1] > 8000]
        self.assertEqual(len(split), 1, "expected one long silence between the echo and the reverb: %r" % found)
        return found[:split[0]], found[split[0]:]

    def test_the_echo_repeats_every_delay_at_half_the_level(self):
        echo, _ = self._phases()
        self.assertEqual(len(echo), 3, "the click and two repeats before the effect stops: %r" % echo)
        dry, first, second = echo
        self.assertGreater(dry[2], CLICK_PEAK * 0.8)
        self.assertLessEqual(abs(dry[2] - CLICK_PEAK), CLICK_PEAK * 0.1)
        # a few samples for the mixer stages between the slot and the DSP output
        c_dry, c_first, c_second = (centroid(self.left, run) for run in echo)
        self.assertTrue(ECHO_DELAY <= c_first - c_dry <= ECHO_DELAY + 8, "first repeat at %.1f" % (c_first - c_dry))
        self.assertAlmostEqual(c_second - c_first, ECHO_DELAY, delta=0.6, msg="the repeats are one delay apart")
        # input gain 0.5 times wet 0.6 on the first repeat, then the feedback of 0.5
        self.assertAlmostEqual(first[2] / dry[2], 0.5 * 0.6, delta=0.03)
        self.assertAlmostEqual(second[2] / first[2], 0.5, delta=0.03)

    def test_the_delay_line_holds_the_written_repeats(self):
        found = bursts(self.ring, 100, circular=True)
        self.assertEqual(len(found), 2, "the two writes inside the ring window: %r" % found)
        (a, _, peak_a), (b, _, peak_b) = sorted(found, key=lambda run: -run[2])
        # the line is written at descending addresses: the later, quieter write sits one delay below
        self.assertEqual((a - b) % RING_WORDS, ECHO_DELAY)
        self.assertAlmostEqual(peak_a, CLICK_PEAK * 0.5 * 0.5, delta=CLICK_PEAK * 0.03)   # in*g_in, then d*fb
        self.assertAlmostEqual(peak_b / peak_a, 0.5, delta=0.03)

    def test_the_reverb_repeats_at_its_four_delays(self):
        _, reverb = self._phases()
        delays = scsp_dsp.REVERB_DELAYS
        self.assertGreaterEqual(len(reverb), 1 + len(delays), "the click and four repeats: %r" % reverb)
        dry, repeats = reverb[0], reverb[1:1 + len(delays)]
        self.assertGreater(dry[2], CLICK_PEAK * 0.8, "a stray pulse at the start of the reverb: %r" % (reverb[:3],))
        c_dry = centroid(self.left, dry)
        offset = centroid(self.left, repeats[0]) - c_dry - delays[0]
        self.assertTrue(0 <= offset <= 8, "first comb at %.1f" % (centroid(self.left, repeats[0]) - c_dry))
        for run, delay in zip(repeats, delays):
            self.assertAlmostEqual(centroid(self.left, run) - c_dry - offset, delay, delta=0.6, msg="a comb at %d" % delay)
            self.assertAlmostEqual(run[2] / dry[2], 0.35 * 0.25, delta=0.02)
        # the first comb comes round again, quieter by its feedback
        again = [run for run in reverb if abs(centroid(self.left, run) - c_dry - offset - 2 * delays[0]) <= 2]
        self.assertEqual(len(again), 1, "the first comb's second pass: %r" % reverb)
        self.assertAlmostEqual(again[0][2] / repeats[0][2], 0.78, delta=0.05)

    def test_the_output_ends_quiet(self):
        tail = self.left[-1500:]
        self.assertLess(max(abs(v) for v in tail), 400)


def _read_wav_left(path):
    """The left channel of a 16-bit PCM WAV, tolerant of a header whose sizes were never finalised."""
    with open(path, "rb") as stream:
        raw = stream.read()
    if raw[:4] != b"RIFF" or raw[8:12] != b"WAVE":
        raise AssertionError("not a WAV file: " + path)
    position = 12
    channels = 2
    rate = 44100
    while position + 8 <= len(raw):
        name = raw[position:position + 4]
        size = struct.unpack("<I", raw[position + 4:position + 8])[0]
        body = position + 8
        if name == b"fmt ":
            channels, rate = struct.unpack("<HI", raw[body + 2:body + 8])
        elif name == b"data":
            data = raw[body:body + size] if body + size <= len(raw) else raw[body:]
            data = data[:len(data) - len(data) % (2 * channels)]
            samples = array.array("h")
            samples.frombytes(data)
            if sys.byteorder == "big":
                samples.byteswap()
            return samples[0::channels], rate
        position = body + size + (size & 1)
    raise AssertionError("no data chunk in " + path)


@unittest.skipUnless(os.environ.get("LIBSATURN_DSP_WAV"), "set LIBSATURN_DSP_WAV to a Mednafen -soundrecord capture")
class RecordedOutputTests(unittest.TestCase):
    """The same checks on a recording of another emulator (Mednafen), which holds the BIOS chime and
    then the demo's sequence repeating. The sequences are found by their shape, not by their time.
    Mednafen resamples to its own rate (48 kHz by default), so distances scale with rate / 44100."""

    @classmethod
    def setUpClass(cls):
        cls.left, cls.rate = _read_wav_left(os.environ["LIBSATURN_DSP_WAV"])
        cls.scale = cls.rate / 44100.0
        cls.found = bursts(cls.left, 60, gap=int(GAP * cls.scale))

    def test_an_echo_sequence_is_present(self):
        found, left, scale = self.found, self.left, self.scale
        delay = ECHO_DELAY * scale
        for i in range(len(found) - 2):
            dry, first, second = found[i:i + 3]
            if not CLICK_PEAK * 0.8 < dry[2] < CLICK_PEAK * 1.1:
                continue
            spacing = (centroid(left, first) - centroid(left, dry), centroid(left, second) - centroid(left, first))
            if (delay <= spacing[0] <= delay + 8 * scale and abs(spacing[1] - delay) <= 2.0 * scale and
                    abs(first[2] / dry[2] - 0.3) < 0.05 and abs(second[2] / first[2] - 0.5) < 0.05):
                return
        self.fail("no click followed by two repeats at the echo delay and level: %r" % (found[:40],))

    def test_a_reverb_sequence_is_present(self):
        found, left, scale = self.found, self.left, self.scale
        delays = [d * scale for d in scsp_dsp.REVERB_DELAYS]
        for i in range(len(found) - len(delays)):
            dry = found[i]
            if not CLICK_PEAK * 0.8 < dry[2] < CLICK_PEAK * 1.1:
                continue
            c_dry = centroid(left, dry)
            repeats = found[i + 1:i + 1 + len(delays)]
            offset = centroid(left, repeats[0]) - c_dry - delays[0]
            if not 0 <= offset <= 8 * scale:
                continue
            if all(abs(centroid(left, run) - c_dry - offset - delay) <= 2.0 * scale and
                   abs(run[2] / dry[2] - 0.35 * 0.25) < 0.02 for run, delay in zip(repeats, delays)):
                return
        self.fail("no click followed by the four reverb repeats: %r" % (found[:40],))


if __name__ == "__main__":
    unittest.main()
