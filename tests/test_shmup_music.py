"""tools/shmup_music.py: the stage loop is deterministic, in format, within headroom and seamless."""
from __future__ import annotations

import sys
import tempfile
import unittest
from pathlib import Path

import numpy as np

REPO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(REPO / "tools"))

import shmup_music as sm


class MusicTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.pcm = sm.render()
        cls.a = np.frombuffer(cls.pcm, ">i2").reshape(-1, 2).astype(np.int32)

    def test_format_and_size(self):
        self.assertEqual(len(self.pcm), sm.FRAMES * 4)
        self.assertEqual(len(self.pcm) % 4, 0)                          # whole stereo frames
        self.assertEqual(sm.RATE, 22050)

    def test_deterministic(self):
        self.assertEqual(sm.render(), self.pcm)

    def test_headroom_and_both_channels_used(self):
        self.assertLess(np.abs(self.a).max(), 32767 * 0.6)
        self.assertGreater(np.abs(self.a).max(), 32767 * 0.4)
        for ch in (0, 1):
            self.assertGreater(np.abs(self.a[:, ch]).mean(), 500)
        self.assertNotEqual(int(np.abs(self.a[:, 0] - self.a[:, 1]).max()), 0)   # real stereo

    def test_loop_seam_is_no_louder_than_the_music(self):
        for ch in (0, 1):
            x = self.a[:, ch]
            step = np.abs(np.diff(x))
            seam = abs(int(x[0]) - int(x[-1]))
            self.assertLessEqual(seam, np.percentile(step, 99.5))

    def test_cli_writes_16_bit_file_and_header(self):
        with tempfile.TemporaryDirectory() as tmp:
            out, hdr = Path(tmp) / "S.PCM", Path(tmp) / "m.h"
            self.assertEqual(sm.main(["--out", str(out), "--header", str(hdr), "--bits", "16"]), 0)
            self.assertEqual(out.read_bytes(), self.pcm)
            self.assertIn(f"BH_MUSIC_BYTES {len(self.pcm)}u", hdr.read_text())
            self.assertIn("BH_MUSIC_SAMPLE_BYTES 2u", hdr.read_text())

    def test_default_is_dithered_8_bit_close_to_the_16_bit_source(self):
        with tempfile.TemporaryDirectory() as tmp:
            out, hdr = Path(tmp) / "S.PCM", Path(tmp) / "m.h"
            self.assertEqual(sm.main(["--out", str(out), "--header", str(hdr)]), 0)
            b = np.frombuffer(out.read_bytes(), np.int8).astype(np.int32)
            self.assertEqual(len(b), sm.FRAMES * 2)                      # interleaved stereo, 1 byte each
            self.assertIn("BH_MUSIC_SAMPLE_BYTES 1u", hdr.read_text())
            self.assertIn(f"BH_MUSIC_BYTES {sm.FRAMES * 2}u", hdr.read_text())
            ref = self.a.reshape(-1).astype(np.float64) / 256.0
            err = np.abs(b - ref)
            self.assertLess(err.max(), 1.6)                              # dither + rounding, never a step more
            signal = np.sqrt(np.mean(ref ** 2))
            noise = np.sqrt(np.mean((b - ref) ** 2))
            self.assertGreater(20 * np.log10(signal / noise), 25.0)      # 8-bit + TPDF dither: ~28 dB here
            self.assertEqual(sm.to_s8(self.pcm), sm.to_s8(self.pcm))     # deterministic


if __name__ == "__main__":
    unittest.main()
