#!/usr/bin/env python3
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2]/'harness/scripts'))
from renderer_pipeline_study import frame_metrics


def row(frame, begin=105, end=115):
    return dict(frame=frame, system_cycles=100+frame*100,
                master_ccr=1, slave_ccr=0, last_begin_cycle=begin, last_end_cycle=end,
                draw_begins=2, draw_ends=2, fb_swaps=4, visible_completed_draws=1)


class FrameMetricsTests(unittest.TestCase):
    def test_repeated_list_does_not_multiply_draw_samples(self):
        result = frame_metrics([row(1), row(2), row(3)])
        self.assertEqual(result['completed_draw_samples'], 1)
        self.assertEqual(result['median_draw_fields'], 0.1)
        self.assertEqual(result['observed_ccr'], dict(master=[1], slave=[0]))
        self.assertEqual(result['counter_deltas']['visible_completed_draws'], 0)

    def test_prior_list_end_is_not_current_list_duration(self):
        result = frame_metrics([row(1), row(2, begin=250, end=115)])
        self.assertEqual(result['completed_draw_cycle_spans'], [10])

    def test_missing_frame_is_rejected(self):
        with self.assertRaises(ValueError):
            frame_metrics([row(1), row(3)])

    def test_stale_clock_is_rejected(self):
        first, second = row(1), row(2)
        second['system_cycles'] = first['system_cycles']
        with self.assertRaises(ValueError):
            frame_metrics([first, second])


if __name__ == '__main__':
    unittest.main()
