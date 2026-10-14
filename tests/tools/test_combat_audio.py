"""Resident combat PCM is deterministic, bounded and exported consistently."""
import sys
from pathlib import Path
import tempfile
import unittest
import wave
sys.path.insert(0,str(Path(__file__).resolve().parents[2]/'tools'))
from generate_combat_audio import effect, generate, DURATIONS, SAMPLE_RATE, PEAK

class CombatAudioTests(unittest.TestCase):
    def test_effects_are_distinct_bounded_and_have_silent_edges(self):
        sounds=[effect(name) for name in DURATIONS]
        self.assertEqual(len({tuple(samples) for samples in sounds}),4)
        for name,samples in zip(DURATIONS,sounds):
            self.assertEqual(samples,effect(name))
            self.assertEqual(len(samples),round(DURATIONS[name]*SAMPLE_RATE))
            self.assertEqual(samples[0],0)
            self.assertEqual(samples[-1],0)
            self.assertLessEqual(max(map(abs,samples)),PEAK)
            self.assertGreater(sum(value*value for value in samples),0)

    def test_bell_has_two_strikes(self):
        samples=effect('bell')
        def energy(begin,end):
            window=samples[round(begin*SAMPLE_RATE):round(end*SAMPLE_RATE)]
            return sum(value*value for value in window)/len(window)
        self.assertGreater(energy(0.19,0.24),energy(0.12,0.17))

    def test_pcm_preview_matches_signed_c_data(self):
        with tempfile.TemporaryDirectory() as directory:
            prefix=Path(directory)/'combat_sfx'
            generate(prefix)
            first=prefix.with_suffix('.c').read_bytes()
            generate(prefix)
            self.assertEqual(first,prefix.with_suffix('.c').read_bytes())
            for name in DURATIONS:
                with wave.open(str(prefix.parent/(name+'.wav')),'rb') as wav:
                    self.assertEqual((wav.getnchannels(),wav.getsampwidth(),wav.getframerate()),(1,1,SAMPLE_RATE))
                    self.assertEqual(wav.readframes(wav.getnframes()),bytes(value+128 for value in effect(name)))

if __name__=='__main__':unittest.main()
