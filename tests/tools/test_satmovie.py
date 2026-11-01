"""tools/satmovie.py on synthetic data: no video file, no ffmpeg."""
import struct
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
import satmovie as pack  # noqa: E402


def ima_decode(blocks):
    """The format LibSaturn's sat_audio_ima_decode_block reads (stereo), decoded back to samples."""
    out = []
    for block in blocks:
        predictor = [struct.unpack_from('>h', block, 0)[0], struct.unpack_from('>h', block, 4)[0]]
        index = [block[2], block[6]]
        frames = [(predictor[0], predictor[1])]
        for f in range(1, pack.AUDIO_BLOCK_FRAMES):
            byte, row = block[8 + f - 1], []
            for c, code in enumerate((byte >> 4, byte & 15)):
                step = pack.IMA_STEPS[index[c]]
                delta = step >> 3
                delta += step if code & 4 else 0
                delta += step >> 1 if code & 2 else 0
                delta += step >> 2 if code & 1 else 0
                predictor[c] = max(-32768, min(32767, predictor[c] + (-delta if code & 8 else delta)))
                index[c] = max(0, min(88, index[c] + pack.IMA_INDEX_DELTA[code & 7]))
                row.append(predictor[c])
            frames.append(tuple(row))
        out.extend(frames)
    return out


class Audio(unittest.TestCase):
    def test_blocks_are_the_size_the_decoder_expects(self):
        pcm = struct.pack('<hh', 0, 0) * (pack.AUDIO_BLOCK_FRAMES * 3)
        blocks = pack.ima_encode(pcm, pack.AUDIO_BLOCK_FRAMES * 3)
        self.assertEqual([len(b) for b in blocks], [1032, 1032, 1032])

    def test_a_tone_comes_back_close(self):
        import math
        frames = pack.AUDIO_BLOCK_FRAMES * 4
        pcm = b''.join(struct.pack('<hh', int(9000 * math.sin(i * 0.12)), int(-7000 * math.sin(i * 0.05)))
                       for i in range(frames))
        decoded = ima_decode(pack.ima_encode(pcm, frames))
        original = struct.unpack(f'<{frames * 2}h', pcm)
        err = sum((decoded[i][c] - original[i * 2 + c]) ** 2 for i in range(frames) for c in (0, 1))
        signal = sum(v * v for v in original)
        self.assertGreater(10 * (signal / err) ** 0.5 if err else 99, 20)       # 4-bit ADPCM: better than ~20 dB

    def test_short_input_is_padded_with_silence(self):
        blocks = pack.ima_encode(struct.pack('<hh', 100, -100), pack.AUDIO_BLOCK_FRAMES)
        decoded = ima_decode(blocks)
        self.assertEqual(decoded[0], (100, -100))
        self.assertEqual(len(decoded), pack.AUDIO_BLOCK_FRAMES)

    def test_the_first_sample_of_a_block_is_exact(self):
        pcm = struct.pack('<hh', 1234, -4321) + struct.pack('<hh', 0, 0) * (pack.AUDIO_BLOCK_FRAMES * 2)
        pcm += struct.pack('<hh', 5555, -777) + struct.pack('<hh', 0, 0) * pack.AUDIO_BLOCK_FRAMES
        blocks = pack.ima_encode(pcm, pack.AUDIO_BLOCK_FRAMES * 3)
        self.assertEqual(struct.unpack_from('>h', blocks[0], 0)[0], 1234)
        self.assertEqual(struct.unpack_from('>h', blocks[0], 4)[0], -4321)


class Frames(unittest.TestCase):
    @staticmethod
    def frame(width=240, height=168, strips=1, chunks=b'\x32\x00\x00\x08abcd'):
        strip = struct.pack('>HHHHHH', 0x1000, len(chunks) + 12, 0, 0, height, width) + chunks
        body = struct.pack('>HHH', width, height, strips) + strip
        return b'\0' + (len(body) + 4).to_bytes(3, 'big') + body

    def test_a_good_frame(self):
        self.assertEqual(pack.check_frame(self.frame()), (240, 168))

    def test_two_strips_are_refused(self):
        with self.assertRaises(ValueError):
            pack.check_frame(self.frame(strips=2))

    def test_grey_chunks_are_refused(self):
        with self.assertRaises(ValueError):
            pack.check_frame(self.frame(chunks=b'\x24\x00\x00\x08abcd'))


class Layout(unittest.TestCase):
    def test_the_movie_file_layout(self):
        frames = [b'\x01\x02\x03\x04\x05', b'\x09' * 8]
        sound = [[b'\xAA' * 1032, b'\xBB' * 1032], []]
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / 'INTRO.MOV'
            pack.write_movie(path, (240, 168), frames, sound, 50000)
            data = path.read_bytes()
        self.assertEqual(data[:8], b'SATMOV01')
        width, height, count, frame_us, rate, channels, codec, block, longest, total = struct.unpack_from('>HHIIIBBHII', data, 8)
        self.assertEqual((width, height, count, frame_us, rate, channels, codec, block), (240, 168, 2, 50000, 22050, 2, 1, 1024))
        first = 2048
        video, audio = struct.unpack_from('>II', data, first)
        self.assertEqual((video, audio), (5, 2064))
        self.assertEqual(data[first + 8:first + 10], b'\xAA\xAA')
        self.assertEqual(data[first + 8 + audio:first + 8 + audio + 5], frames[0])
        second = first + 8 + audio + 8                    # five bytes of video padded to eight
        self.assertEqual(struct.unpack_from('>II', data, second), (8, 0))
        self.assertEqual(total, second + 8 + 8 - first)
        self.assertEqual(longest, 8 + 2064 + 8)
        self.assertEqual(len(data), 2048 + total)

    def test_a_drive_that_cannot_keep_up_stops_the_build(self):
        with tempfile.TemporaryDirectory() as tmp:
            heavy = [b'\0' * 40000] * 40                  # 800 KB/s
            with self.assertRaises(SystemExit):
                pack.write_movie(Path(tmp) / 'x.MOV', (240, 168), heavy, [[] for _ in heavy], 50000)

    def test_slack_and_window_rate(self):
        sizes = [10000] * 100
        self.assertEqual(pack.drive_slack(sizes, 50000, rate=300000, preroll=0), 15000 * 0 + min(
            300000 * k * 0.05 - 10000 * (k + 1) for k in range(100)))
        self.assertAlmostEqual(pack.worst_window_rate(sizes, 50000, seconds=2), 200000.0)


if __name__ == '__main__':
    unittest.main()
