#!/usr/bin/env python3
"""Convert an audio source to a CD stream and emit matching C metadata.

S16BE keeps the original PCM path. S8 supplies small resident SCSP samples;
optional clipping and loop crossfade prepare seamless sample banks. IMA ADPCM
stores independently decodable 1024-frame blocks, then sat_music expands each
block to S16BE in RAM.
"""

from __future__ import annotations

import argparse
import re
import struct
import subprocess
import sys
from array import array
from pathlib import Path


BLOCK_FRAMES = 1024
STEP_TABLE = (
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31,
    34, 37, 41, 45, 50, 55, 60, 66, 73, 80, 88, 97, 107, 118,
    130, 143, 157, 173, 190, 209, 230, 253, 279, 307, 337, 371,
    408, 449, 494, 544, 598, 658, 724, 796, 876, 963, 1060, 1166,
    1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024,
    3327, 3660, 4026, 4428, 4871, 5358, 5894, 6484, 7132, 7845,
    8630, 9493, 10442, 11487, 12635, 13899, 15289, 16818, 18500,
    20350, 22385, 24623, 27086, 29794, 32767,
)
INDEX_DELTA = (-1, -1, -1, -1, 2, 4, 6, 8)


def block_bytes(channels: int) -> int:
    payload = ((BLOCK_FRAMES - 1) * channels + 1) // 2
    return (4 * channels + payload + 3) & ~3


def encode_sample(sample: int, predictor: int, index: int) -> tuple[int, int, int]:
    step = STEP_TABLE[index]
    difference = sample - predictor
    code = 0
    if difference < 0:
        code = 8
        difference = -difference
    magnitude = 0
    if difference >= step:
        magnitude |= 4
        difference -= step
    if difference >= step >> 1:
        magnitude |= 2
        difference -= step >> 1
    if difference >= step >> 2:
        magnitude |= 1
    code |= magnitude
    delta = step >> 3
    if magnitude & 4:
        delta += step
    if magnitude & 2:
        delta += step >> 1
    if magnitude & 1:
        delta += step >> 2
    predictor += -delta if code & 8 else delta
    predictor = max(-32768, min(32767, predictor))
    index = max(0, min(88, index + INDEX_DELTA[magnitude]))
    return code, predictor, index


def encode_adpcm(pcm: bytes, channels: int) -> bytes:
    samples = array("h")
    samples.frombytes(pcm)
    if sys.byteorder != "big":
        samples.byteswap()
    frames = len(samples) // channels
    size = block_bytes(channels)
    output = bytearray(((frames + BLOCK_FRAMES - 1) // BLOCK_FRAMES) * size)
    previous_index = [0] * channels
    for block in range((frames + BLOCK_FRAMES - 1) // BLOCK_FRAMES):
        start_frame = block * BLOCK_FRAMES
        block_start = block * size
        for channel in range(channels):
            first = samples[start_frame * channels + channel]
            struct.pack_into(">hBB", output, block_start + 4 * channel,
                             first, previous_index[channel], 0)
        predictors = [samples[start_frame * channels + c] for c in range(channels)]
        nibble_number = 0
        for frame in range(1, BLOCK_FRAMES):
            for channel in range(channels):
                source_frame = min(start_frame + frame, frames - 1)
                sample = samples[source_frame * channels + channel]
                code, predictors[channel], previous_index[channel] = encode_sample(
                    sample, predictors[channel], previous_index[channel]
                )
                at = block_start + 4 * channels + nibble_number // 2
                if nibble_number & 1:
                    output[at] |= code
                else:
                    output[at] = code << 4
                nibble_number += 1
    return bytes(output)


def loop_crossfade(pcm: bytes, channels: int, frames_to_blend: int) -> bytes:
    if frames_to_blend == 0:
        return pcm
    samples = array("h")
    samples.frombytes(pcm)
    if sys.byteorder != "big":
        samples.byteswap()
    frames = len(samples) // channels
    if frames_to_blend * 2 >= frames:
        raise ValueError("loop crossfade must be shorter than half the clip")
    output = samples[frames_to_blend * channels:]
    tail_start = (frames - 2 * frames_to_blend) * channels
    for frame in range(frames_to_blend):
        for channel in range(channels):
            at = tail_start + frame * channels + channel
            head = samples[frame * channels + channel]
            output[at] = (output[at] * (frames_to_blend - frame - 1) +
                          head * (frame + 1)) // frames_to_blend
    if sys.byteorder != "big":
        output.byteswap()
    return output.tobytes()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", required=True, type=Path)
    parser.add_argument("--output-audio", required=True, type=Path)
    parser.add_argument("--output-header", required=True, type=Path)
    parser.add_argument("--name", required=True)
    parser.add_argument("--sample-rate", required=True, type=int)
    parser.add_argument("--channels", required=True, type=int)
    parser.add_argument("--format", choices=("s16be", "s8", "ima_adpcm"), default="s16be")
    parser.add_argument("--start-ms", type=int, default=0)
    parser.add_argument("--duration-ms", type=int, default=0)
    parser.add_argument("--loop-crossfade-ms", type=int, default=0)
    parser.add_argument("--ffmpeg", default="ffmpeg")
    args = parser.parse_args()

    if args.sample_rate <= 0 or args.channels not in (1, 2):
        parser.error("sample rate must be positive and channels must be 1 or 2")
    if not args.input.is_file():
        parser.error(f"audio source not found: {args.input}")
    if min(args.start_ms, args.duration_ms, args.loop_crossfade_ms) < 0:
        parser.error("clip times must be nonnegative")

    args.output_audio.parent.mkdir(parents=True, exist_ok=True)
    args.output_header.parent.mkdir(parents=True, exist_ok=True)
    command = [
        args.ffmpeg, "-v", "error", "-i", str(args.input),
        "-ar", str(args.sample_rate), "-ac", str(args.channels),
        "-c:a", "pcm_s16be", "-f", "s16be", "-",
    ]
    try:
        pcm = subprocess.run(command, check=True, stdout=subprocess.PIPE).stdout
    except FileNotFoundError:
        parser.error(f"FFmpeg executable not found: {args.ffmpeg}")
    except subprocess.CalledProcessError as exc:
        parser.error(f"FFmpeg failed with exit code {exc.returncode}")

    frame_bytes = args.channels * 2
    if not pcm or len(pcm) % frame_bytes:
        parser.error(f"invalid S16BE output length {len(pcm)}")
    first_frame = args.start_ms * args.sample_rate // 1000
    end_frame = len(pcm) // frame_bytes
    if args.duration_ms:
        end_frame = min(end_frame, first_frame + args.duration_ms * args.sample_rate // 1000)
    if first_frame >= end_frame:
        parser.error("audio clip is empty")
    pcm = pcm[first_frame * frame_bytes:end_frame * frame_bytes]
    try:
        pcm = loop_crossfade(pcm, args.channels,
                             args.loop_crossfade_ms * args.sample_rate // 1000)
    except ValueError as exc:
        parser.error(str(exc))
    sample_count = len(pcm) // frame_bytes
    encoded = (encode_adpcm(pcm, args.channels) if args.format == "ima_adpcm"
               else pcm[0::2] if args.format == "s8" else pcm)
    args.output_audio.write_bytes(encoded)
    prefix = "SAT_AUDIO_" + re.sub(r"[^A-Za-z0-9]+", "_", args.name).strip("_").upper()
    guard = prefix + "_H"
    format_macro = {"ima_adpcm": "SAT_AUDIO_IMA_ADPCM",
                    "s8": "SAT_AUDIO_PCM_S8", "s16be": "SAT_AUDIO_PCM_S16"}[args.format]
    args.output_header.write_text(
        "\n".join([
            f"#ifndef {guard}", f"#define {guard}", "",
            f"#define {prefix}_SAMPLE_RATE {args.sample_rate}u",
            f"#define {prefix}_CHANNELS {args.channels}u",
            f"#define {prefix}_SAMPLE_COUNT {sample_count}u",
            f"#define {prefix}_BYTES {len(encoded)}u",
            f"#define {prefix}_FORMAT {format_macro}",
            "", f"#endif /* {guard} */", "",
        ]),
        encoding="ascii",
    )
    print(
        f"[audio] {args.name}: {sample_count} frames, {len(encoded)} bytes, "
        f"{args.sample_rate} Hz, {args.channels} channel(s), {args.format}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
