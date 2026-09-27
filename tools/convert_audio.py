#!/usr/bin/env python3
"""Convert an audio source into raw S16BE PCM and a C metadata header.

The input format is selected by FFmpeg from its filename/content. This is a
generic build helper for CD-backed ``sat_music`` assets; examples declare the
source, output path, sample rate and channel count in their Makefile.inc.
"""

from __future__ import annotations

import argparse
import re
import subprocess
from pathlib import Path


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", required=True, type=Path)
    parser.add_argument("--output-pcm", required=True, type=Path)
    parser.add_argument("--output-header", required=True, type=Path)
    parser.add_argument("--name", required=True)
    parser.add_argument("--sample-rate", required=True, type=int)
    parser.add_argument("--channels", required=True, type=int)
    parser.add_argument("--ffmpeg", default="ffmpeg")
    args = parser.parse_args()

    if args.sample_rate <= 0 or args.channels not in (1, 2):
        parser.error("sample rate must be positive and channels must be 1 or 2")
    if not args.input.is_file():
        parser.error(f"audio source not found: {args.input}")

    args.output_pcm.parent.mkdir(parents=True, exist_ok=True)
    args.output_header.parent.mkdir(parents=True, exist_ok=True)
    command = [
        args.ffmpeg,
        "-v", "error", "-y", "-i", str(args.input),
        "-ar", str(args.sample_rate), "-ac", str(args.channels),
        "-c:a", "pcm_s16be", "-f", "s16be", str(args.output_pcm),
    ]
    try:
        subprocess.run(command, check=True)
    except FileNotFoundError:
        parser.error(f"FFmpeg executable not found: {args.ffmpeg}")
    except subprocess.CalledProcessError as exc:
        parser.error(f"FFmpeg failed with exit code {exc.returncode}")

    byte_count = args.output_pcm.stat().st_size
    frame_bytes = args.channels * 2
    if byte_count == 0 or byte_count % frame_bytes:
        args.output_pcm.unlink(missing_ok=True)
        parser.error(
            f"invalid S16BE output length {byte_count}; expected a nonzero "
            f"multiple of {frame_bytes} bytes"
        )

    prefix = "SAT_AUDIO_" + re.sub(r"[^A-Za-z0-9]+", "_", args.name).strip("_").upper()
    guard = prefix + "_H"
    sample_count = byte_count // frame_bytes
    args.output_header.write_text(
        "\n".join([
            f"#ifndef {guard}", f"#define {guard}", "",
            f"#define {prefix}_SAMPLE_RATE {args.sample_rate}u",
            f"#define {prefix}_CHANNELS {args.channels}u",
            f"#define {prefix}_SAMPLE_COUNT {sample_count}u",
            f"#define {prefix}_BYTES {byte_count}u",
            "", f"#endif /* {guard} */", "",
        ]),
        encoding="ascii",
    )
    print(
        f"[audio] {args.name}: {sample_count} frames, {byte_count} bytes, "
        f"{args.sample_rate} Hz, {args.channels} channel(s), S16BE"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
