"""Float32 clip endpoints must not lengthen baked looping animations."""
import struct
import sys
import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tools'))
from model_pipeline.animated_import import animated_frame_times, rate_fraction


class AnimationSampleTimesTests(unittest.TestCase):
    def test_source_rate_fits_runtime_fields_after_float32_rounding(self):
        duration, = struct.unpack('<f', struct.pack('<f', 40 / 30))
        self.assertEqual(rate_fraction(32, duration), (24, 1))
        self.assertEqual(rate_fraction(20, duration), (15, 1))

    def test_fractional_ntsc_rate_is_preserved(self):
        self.assertEqual(rate_fraction(60, 1.001), (60000, 1001))

    def test_float32_stride_retains_twenty_samples(self):
        duration, = struct.unpack('<f', struct.pack('<f', 40 / 30))
        for end in (duration, 40 / 30, 40 / 30 - 1e-8):
            with self.subTest(duration=end), patch(
                'model_pipeline.animated_import.anim_eval.bake_clip_poses',
                return_value=[[(0, 0, 0)], [(0, 0, 0)]],
            ):
                times, loop = animated_frame_times(None, SimpleNamespace(duration=end), '15')
                self.assertTrue(loop)
                self.assertEqual(len(times), 20)
                self.assertEqual(times, [i / 15 for i in range(20)])

    def test_nonintegral_terminal_sample_is_kept_for_one_shot(self):
        with patch(
            'model_pipeline.animated_import.anim_eval.bake_clip_poses',
            return_value=[[(0, 0, 0)], [(1, 0, 0)]],
        ):
            times, loop = animated_frame_times(None, SimpleNamespace(duration=1.35), '15')
            self.assertFalse(loop)
            self.assertEqual(len(times), 22)
            self.assertEqual(times[-1], 1.35)


if __name__ == '__main__':
    unittest.main()
