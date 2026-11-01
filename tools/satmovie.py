#!/usr/bin/env python3
"""Convert a video to a LibSaturn movie (.MOV): Cinepak picture and IMA ADPCM sound in one file.

    satmovie.py INPUT -o NAME.MOV [--size 240x168] [--fps 20] [--quality 20]

ffmpeg does the decoding, scaling and Cinepak encoding (one strip per frame, which is what the Saturn decoder
reads); the sound becomes 22.05 kHz stereo IMA ADPCM (the blocks `sat_audio_ima_decode_block` reads) and is
interleaved a little ahead of the frame it plays with. The result plays through `sat_movie_play`
(saturn/movie.h). The file is refused when a 2x CD drive (less 10% for retries) could not keep up with it.

File layout, all values big-endian:

  sector 0 (2048 bytes)   "SATMOV01", u16 width, u16 height, u32 frames, u32 frame_us, u32 audio rate,
                          u8 audio channels (2), u8 audio codec (1 = IMA ADPCM, 0 = none), u16 audio block frames
                          (1024), u32 longest packet, u32 packet bytes; the rest zero.
  from sector 1           one packet per frame: u32 video bytes, u32 audio bytes, the audio blocks (1032 bytes
                          each), the Cinepak frame, zeros to a multiple of 4.

The functions below are also the library for tools that cut a movie from somewhere else (a game's own disc).
"""
import argparse
import array
import struct
import subprocess
import sys
from pathlib import Path

MAGIC = b'SATMOV01'
HEADER_BYTES = 2048
AUDIO_RATE, AUDIO_CHANNELS, AUDIO_BLOCK_FRAMES = 22050, 2, 1024
AUDIO_LEAD_US = 600000              # a block travels this far ahead of the moment it starts to play
DRIVE_BYTES_PER_SECOND = 150 * 2048 * 9 // 10   # a 2x drive at 150 sectors a second, less 10% for retries
PREROLL_BYTES = 24 * 2048           # what the player has read when its clock starts
IMA_STEPS = (7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45, 50, 55, 60, 66, 73, 80,
             88, 97, 107, 118, 130, 143, 157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408, 449, 494, 544, 598,
             658, 724, 796, 876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327,
             3660, 4026, 4428, 4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899, 15289,
             16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767)
IMA_INDEX_DELTA = (-1, -1, -1, -1, 2, 4, 6, 8)


def ffmpeg(*args):
    subprocess.run(['ffmpeg', '-y', '-loglevel', 'error', *map(str, args)], check=True)


def encode_cinepak(input_args, size, quality, out, fps=None):
    """Encode `input_args` (ffmpeg input options and file) to a Cinepak AVI, one strip per frame."""
    w, h = size
    if w % 4 or h % 4:
        raise ValueError('the picture size must be a multiple of 4 each way')
    scale = f'scale={w}:{h}:flags=lanczos' + (f',fps={fps}' if fps else '')
    ffmpeg(*input_args, '-an', '-fps_mode', 'passthrough' if not fps else 'cfr', '-vf', scale, '-pix_fmt', 'rgb24',
           '-c:v', 'cinepak', '-q:v', quality, '-max_strips', 1, '-skip_empty_cb', 1, out)


def split_frames(avi, scratch):
    """The encoder's packets, one per frame, in order."""
    raw = Path(scratch) / (Path(avi).stem + '.cvid')
    ffmpeg('-i', avi, '-c:v', 'copy', '-f', 'rawvideo', raw)
    sizes = subprocess.run(['ffprobe', '-v', 'error', '-select_streams', 'v', '-show_entries', 'packet=size',
                            '-of', 'csv=p=0', avi], check=True, capture_output=True, text=True).stdout.split()
    data, frames, pos = raw.read_bytes(), [], 0
    for size in map(int, sizes):
        frames.append(data[pos:pos + size])
        pos += size
    raw.unlink()
    return frames


def check_frame(frame):
    """The Saturn decoder reads one strip of colour chunks; a frame that is not is an error here, not on the console."""
    width, height, strips = struct.unpack_from('>HHH', frame, 4)
    if strips != 1:
        raise ValueError(f'{strips} strips: encode with -max_strips 1')
    end, pos = 10 + struct.unpack_from('>H', frame, 12)[0], 22
    while end - pos >= 4:
        if 0x24 <= frame[pos] <= 0x27:
            raise ValueError('grey chunk')
        pos += struct.unpack_from('>H', frame, pos + 2)[0]
    return width, height


def decode_audio(input_args, out):
    """Stereo S16LE at the movie's rate from `input_args`; empty when the input has no sound."""
    result = subprocess.run(['ffmpeg', '-y', '-loglevel', 'error', *map(str, input_args), '-vn', '-ar', str(AUDIO_RATE),
                             '-ac', str(AUDIO_CHANNELS), '-f', 's16le', str(out)])
    if result.returncode != 0:
        return b''
    data = Path(out).read_bytes()
    Path(out).unlink()
    return data


def ima_encode(pcm, frames_wanted):
    """Stereo S16LE to blocked IMA ADPCM blocks of AUDIO_BLOCK_FRAMES frames (silence pads, the end cuts)."""
    samples = array.array('h')
    samples.frombytes(pcm[:frames_wanted * 4])
    samples.extend([0] * (frames_wanted * 2 - len(samples)))
    blocks, index, predictor = [], [0, 0], [0, 0]
    for first in range(0, frames_wanted, AUDIO_BLOCK_FRAMES):
        block = bytearray()
        for c in (0, 1):
            predictor[c] = samples[first * 2 + c] if first * 2 + c < len(samples) else 0
            block += struct.pack('>hBB', predictor[c], index[c], 0)
        for f in range(1, AUDIO_BLOCK_FRAMES):
            byte = 0
            for c in (0, 1):
                at = (first + f) * 2 + c
                target = samples[at] if at < len(samples) else 0
                diff, code = target - predictor[c], 0
                if diff < 0:
                    code, diff = 8, -diff
                step = IMA_STEPS[index[c]]
                if diff >= step:
                    code |= 4
                    diff -= step
                if diff >= step >> 1:
                    code |= 2
                    diff -= step >> 1
                if diff >= step >> 2:
                    code |= 1
                delta = step >> 3
                delta += step if code & 4 else 0
                delta += step >> 1 if code & 2 else 0
                delta += step >> 2 if code & 1 else 0
                predictor[c] = max(-32768, min(32767, predictor[c] - delta if code & 8 else predictor[c] + delta))
                index[c] = max(0, min(88, index[c] + IMA_INDEX_DELTA[code & 7]))
                byte = byte << 4 | code
            block.append(byte)
        blocks.append(bytes(block).ljust(4 * ((len(block) + 3) // 4), b'\0'))
    return blocks


def spread_audio(pcm, frames, frame_us):
    """The audio blocks to carry in each video packet: a list of lists, one per frame."""
    wanted = -(-frames * frame_us * AUDIO_RATE // 1000000 // AUDIO_BLOCK_FRAMES) * AUDIO_BLOCK_FRAMES
    lead = -(-AUDIO_LEAD_US // frame_us)
    carried = [[] for _ in range(frames)]
    if not pcm:
        return carried
    for b, block in enumerate(ima_encode(pcm, wanted)):
        due = b * AUDIO_BLOCK_FRAMES * 1000000 // AUDIO_RATE // frame_us          # the frame it starts to play in
        carried[max(0, min(frames - 1, due - lead))].append(block)
    return carried


def drive_slack(packet_sizes, frame_us, rate=DRIVE_BYTES_PER_SECOND, preroll=PREROLL_BYTES):
    """Fewest bytes the drive is ahead of the player, as the player consumes packet k at frame k's time.

    Negative means the player would wait for the disc somewhere. It counts the preroll and the drive's rate,
    not the CD Block's 200-sector buffer, which is extra cushion on top."""
    total, worst = 0, None
    for k, size in enumerate(packet_sizes):
        total += size
        slack = preroll + rate * k * frame_us / 1000000 - total
        worst = slack if worst is None else min(worst, slack)
    return worst


def worst_window_rate(packet_sizes, frame_us, seconds=10):
    """Highest data rate over any `seconds` of the movie, in bytes a second."""
    window = min(max(1, seconds * 1000000 // frame_us), len(packet_sizes))
    return max(sum(packet_sizes[i:i + window]) for i in range(len(packet_sizes) - window + 1)) * 1000000 / (window * frame_us)


def write_movie(path, size, frames, audio, frame_us):
    """Write the file. SystemExit when a 2x drive could not read it in time."""
    packets, longest, sizes = bytearray(), 0, []
    for frame, blocks in zip(frames, audio):
        sound = b''.join(blocks)
        packet = struct.pack('>II', len(frame), len(sound)) + sound + frame + b'\0' * (-len(frame) % 4)
        longest = max(longest, len(packet))
        packets += packet
        sizes.append(len(packet))
    slack = drive_slack(sizes, frame_us)
    if slack < 0:
        raise SystemExit(f'the drive cannot keep up: {-slack / 1024:.0f} KiB short at 90% of 2x; lower --quality or the size')
    has_sound = any(audio)
    print(f'data rate: mean {len(packets) * 1000000 / (len(frames) * frame_us) / 1024:.0f} KiB/s, worst 10 s '
          f'{worst_window_rate(sizes, frame_us) / 1024:.0f} KiB/s of {150 * 2048 / 1024:.0f} at 2x; ahead of the drive by '
          f'at least {slack / 1024:.0f} KiB')
    header = MAGIC + struct.pack('>HHIIIBBHII', size[0], size[1], len(frames), frame_us,
                                 AUDIO_RATE if has_sound else 0, AUDIO_CHANNELS if has_sound else 0, 1 if has_sound else 0,
                                 AUDIO_BLOCK_FRAMES if has_sound else 0, longest, len(packets))
    Path(path).write_bytes(header.ljust(HEADER_BYTES, b'\0') + packets)


def convert(input_path, out, size, fps, quality, scratch):
    """A video file to a movie file."""
    scratch = Path(scratch)
    scratch.mkdir(parents=True, exist_ok=True)
    avi = scratch / (Path(out).stem + '.avi')
    encode_cinepak(['-i', input_path], size, quality, avi, fps=fps)
    frames = split_frames(avi, scratch)
    avi.unlink()
    for frame in frames:
        got = check_frame(frame)
        if got != tuple(size):
            raise SystemExit(f'frame is {got}, expected {size}')
    frame_us = round(1000000 / fps)
    pcm = decode_audio(['-i', input_path], scratch / (Path(out).stem + '.pcm'))
    write_movie(out, size, frames, spread_audio(pcm, len(frames), frame_us), frame_us)
    print(f'{out}: {len(frames)} frames of {size[0]}x{size[1]} at {fps} fps, {Path(out).stat().st_size / 1048576:.2f} MiB')


def main():
    p = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    p.add_argument('input', type=Path)
    p.add_argument('-o', '--out', type=Path, required=True)
    p.add_argument('--size', default='240x168', help='WIDTHxHEIGHT, multiples of 4, at most 320x224')
    p.add_argument('--fps', type=float, default=20.0)
    p.add_argument('--quality', type=int, default=20, help='ffmpeg cinepak -q:v: higher is smaller')
    p.add_argument('--scratch', type=Path, default=Path('satmovie-work'))
    args = p.parse_args()
    width, height = (int(v) for v in args.size.lower().split('x'))
    if width > 320 or height > 224:
        p.error('the player shows at most 320x224')
    convert(args.input, args.out, (width, height), args.fps, args.quality, args.scratch)
    if not any(args.scratch.iterdir()):
        args.scratch.rmdir()


if __name__ == '__main__':
    sys.exit(main())
