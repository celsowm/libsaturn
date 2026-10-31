"""Mixed-mode packaging must preserve ISO sectors and exact stereo PCM bytes."""
import sys
import tempfile
import unittest
import shutil
import struct
import wave
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tools'))
from build_cd_audio import build, PREGAP_SECTORS
from iso_to_raw import validate_mode1_sector
from convert_cd_audio import convert


class CdAudioDiscTests(unittest.TestCase):
    @unittest.skipUnless(shutil.which('ffmpeg'), 'FFmpeg is required for CD source conversion')
    def test_conversion_preserves_complete_pcm_source_including_last_sample(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            source, output = root / 'reference.wav', root / 'track.pcm'
            pcm = b''.join(struct.pack('<hh', n - 500, 1000 - n) for n in range(1001))
            with wave.open(str(source), 'wb') as wav:
                wav.setparams((2, 2, 44100, 0, 'NONE', 'not compressed'))
                wav.writeframes(pcm)
            report = convert(source, output, shutil.which('ffmpeg'))
            result = output.read_bytes()
            self.assertEqual(result[:len(pcm)], pcm)
            self.assertEqual(result[len(pcm):], bytes(len(result) - len(pcm)))
            self.assertEqual(len(result) % 2352, 0)
            self.assertEqual(report['source_pcm_frames'], 1001)
            self.assertLess(report['padding_frames'], 588)

    def test_exact_data_pcm_and_track_boundaries(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            iso, audio, raw, cue = [root / n for n in ('test.iso', 'source.cdda', 'test.bin', 'test.cue')]
            data = bytes(range(256)) * 16
            # Asymmetric stereo signed samples expose byte swaps/channel swaps.
            pcm = b'\x01\x80\xfe\x7f' * 1176
            iso.write_bytes(data)
            audio.write_bytes(pcm)
            result = build(iso, audio, raw, cue)
            contents = raw.read_bytes()
            for lba in range(2):
                self.assertEqual(validate_mode1_sector(lba, contents[lba * 2352:(lba + 1) * 2352]),
                                 data[lba * 2048:(lba + 1) * 2048])
            self.assertEqual(contents[2 * 2352:(2 + PREGAP_SECTORS) * 2352], bytes(150 * 2352))
            self.assertEqual(contents[(2 + PREGAP_SECTORS) * 2352:], pcm)
            self.assertEqual(result['audio_sectors'], 2)
            self.assertIn('TRACK 02 AUDIO\n    INDEX 00 00:00:02\n    INDEX 01 00:02:02', cue.read_text())

    def test_partial_audio_sector_is_rejected_before_publication(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            iso, audio, raw, cue = [root / n for n in ('test.iso', 'source.cdda', 'test.bin', 'test.cue')]
            iso.write_bytes(bytes(2048))
            audio.write_bytes(bytes(2353))
            with self.assertRaises(ValueError):
                build(iso, audio, raw, cue)
            self.assertFalse(raw.exists())
            self.assertFalse(cue.exists())

    def test_source_cannot_be_overwritten(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            iso, audio = root / 'test.iso', root / 'source.cdda'
            iso.write_bytes(bytes(2048))
            audio.write_bytes(bytes(2352))
            with self.assertRaises(ValueError):
                build(iso, audio, audio, root / 'test.cue')
            self.assertEqual(audio.stat().st_size, 2352)


if __name__ == '__main__':
    unittest.main()
