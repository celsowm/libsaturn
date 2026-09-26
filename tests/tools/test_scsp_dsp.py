"""Tests for tools/scsp_dsp.py: field encoding, the simulator and the echo/reverb presets."""

import os
import sys
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "tools"))
import scsp_dsp as d  # noqa: E402


def run(program, coef, madrs, ring_length, samples, mixs_index=0):
    """Feeds `samples` (16-bit values) into MIXS 0 and returns EFREG 0 after each sample."""
    dsp = d.Dsp(program, coef, madrs, ring_length)
    out = []
    for value in samples:
        dsp.run_sample({mixs_index: value << 4})
        out.append(dsp.effect[0])
    return out, dsp


class FieldTests(unittest.TestCase):
    def test_fields_land_on_their_bits(self):
        self.assertEqual(d.step(IWT=1), 1 << 37)
        self.assertEqual(d.step(IRA=0x20), 0x20 << 38)
        self.assertEqual(d.step(TWA=5, TWT=1, TRA=3), (5 << 48) | (1 << 55) | (3 << 56))
        self.assertEqual(d.step(XSEL=1, YSEL=3), (1 << 47) | (3 << 45))

    def test_out_of_range_fields_are_refused(self):
        with self.assertRaises(ValueError):
            d.step(IWA=32)
        with self.assertRaises(ValueError):
            d.step(YSEL=4)

    def test_coefficients_are_13_bit_twos_complement(self):
        self.assertEqual(d.coefficient(0.5), 0x0800)
        self.assertEqual(d.coefficient(-0.5), 0x1800)
        self.assertEqual(d.coefficient(1.0), 0x0FFF)        # 1.0 does not fit; the largest gain is used
        self.assertEqual(d.coefficient(0.0), 0)


class SimulatorTests(unittest.TestCase):
    def test_a_gain_scales_the_input(self):
        program = [d.step(IRA=d.IRA_MIXS, XSEL=1, YSEL=1, CRA=0, ZERO=1),
                   d.step(EWT=1, EWA=0, ZERO=1, YSEL=1, CRA=63)]
        coef = [d.coefficient(0.5)]
        out, _ = run(program, coef, [], 0, [1000])
        self.assertEqual(out[0], 500)

    def test_memory_write_reads_back_after_the_delay(self):
        # write the input to the ring, read it back three samples later
        write = d.step(IRA=d.IRA_MIXS, XSEL=1, YSEL=1, CRA=0, ZERO=1, NOFL=1, MASA=1)
        program = [write,
                   d.step(MWT=1, NOFL=1, MASA=1, ZERO=1, YSEL=1, CRA=63),
                   d.step(MRD=1, NOFL=1, MASA=0, ZERO=1, YSEL=1, CRA=63),
                   d.step(ZERO=1, YSEL=1, CRA=63),
                   d.step(IWT=1, IWA=0, ZERO=1, YSEL=1, CRA=63),
                   d.step(IRA=0, XSEL=0, ZERO=1, YSEL=1, CRA=63),
                   d.step(IRA=0, XSEL=1, YSEL=1, CRA=0, ZERO=1),
                   d.step(EWT=1, EWA=0, ZERO=1, YSEL=1, CRA=63)]
        coef = [0x0FFF]
        out, _ = run(program, coef, [3, 0], 0, [1000, 0, 0, 0, 0, 0])
        self.assertEqual([abs(v) for v in out].index(max(abs(v) for v in out)), 3)
        self.assertAlmostEqual(out[3], 1000, delta=2)


class EchoTests(unittest.TestCase):
    def test_impulse_response_is_a_decaying_train_at_the_delay(self):
        delay, fb, wet, gin = 100, 0.5, 0.6, 0.5
        coef, madrs = d.echo_data(delay, fb, wet, gin)
        impulse = [20000] + [0] * 599
        out, _ = run(d.echo_program(), coef, madrs, d.echo_ring_length(delay), impulse)
        self.assertEqual(out[0], 0)
        for k in range(1, 6):
            expected = 20000 * gin * fb ** (k - 1) * wet
            self.assertAlmostEqual(out[k * delay], expected, delta=max(3.0, expected * 0.01), msg="echo %d" % k)
        quiet = [i for i, v in enumerate(out) if abs(v) > 3 and i % delay != 0 and i < 600]
        self.assertEqual(quiet, [], "energy between the echoes")

    def test_a_long_delay_uses_a_bigger_ring(self):
        self.assertEqual(d.echo_ring_length(2205), 0)
        self.assertEqual(d.echo_ring_length(8192), 1)
        self.assertEqual(d.echo_ring_length(44100), 3)
        with self.assertRaises(ValueError):
            d.echo_ring_length(65536)
        delay = 9000
        coef, madrs = d.echo_data(delay, 0.5, 0.6, 0.5)
        impulse = [20000] + [0] * (delay + 5)
        out, _ = run(d.echo_program(), coef, madrs, d.echo_ring_length(delay), impulse)
        self.assertAlmostEqual(out[delay], 20000 * 0.5 * 0.6, delta=6)

    def test_zero_feedback_gives_a_single_echo(self):
        coef, madrs = d.echo_data(50, 0.0, 0.6, 0.5)
        out, _ = run(d.echo_program(), coef, madrs, 0, [20000] + [0] * 300)
        self.assertGreater(out[50], 5000)
        self.assertEqual([i for i, v in enumerate(out) if abs(v) > 3 and i != 50], [])

    def test_the_program_fits(self):
        self.assertLessEqual(len(d.echo_program()), d.STEPS)
        self.assertLessEqual(len(d.reverb_program()), d.STEPS)


class ReverbTests(unittest.TestCase):
    def test_four_echoes_then_a_dense_decaying_tail(self):
        fb, wet, gin = 0.78, 0.25, 0.35
        coef, madrs = d.reverb_data(d.REVERB_DELAYS, fb, wet, gin)
        impulse = [20000] + [0] * 8500
        out, _ = run(d.reverb_program(), coef, madrs, d.REVERB_RING_LENGTH, impulse)
        first = 20000 * gin * wet
        for delay in d.REVERB_DELAYS:
            self.assertAlmostEqual(out[delay], first, delta=first * 0.02, msg="comb at %d" % delay)
        self.assertEqual(max(abs(v) for v in out[:d.REVERB_DELAYS[0]]), 0)
        # each comb repeats every delay samples at feedback times the amplitude
        for delay in d.REVERB_DELAYS:
            self.assertAlmostEqual(out[2 * delay], first * fb, delta=first * 0.05)

    def test_reverb_tail_decays(self):
        coef, madrs = d.reverb_data()
        impulse = [20000] + [0] * 39999
        out, _ = run(d.reverb_program(), coef, madrs, d.REVERB_RING_LENGTH, impulse)
        early = max(abs(v) for v in out[:6000])
        late = max(abs(v) for v in out[34000:])
        self.assertGreater(early, 1000)
        self.assertLess(late, early / 4)

    def test_delays_are_validated(self):
        with self.assertRaises(ValueError):
            d.reverb_data((1, 2, 3, 5000))
        with self.assertRaises(ValueError):
            d.reverb_data((1, 2, 3))


class HeaderTests(unittest.TestCase):
    def test_the_committed_header_is_current(self):
        self.assertEqual(d.main(["header", "--check"]), 0)


if __name__ == "__main__":
    unittest.main()
