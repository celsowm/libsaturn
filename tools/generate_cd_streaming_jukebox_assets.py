#!/usr/bin/env python3
"""Stage the CD-only PCM tracks for cd_streaming_jukebox.

All three tracks are freely licensed renders from Wikimedia, vendored as
build-ready big-endian S16 stereo @ 44100 Hz (see
examples/cd_streaming_jukebox/audio-src/ and ../LICENSES.md). This tool stages
a 22050 Hz stereo version whose data rate leaves headroom for the Saturn CD
Block's command latency, and emits the sizing header without external codecs.
"""

import argparse
from pathlib import Path

SOURCE_RATE = 44100
RATE = 22050
CHANNELS = 2
# Vendored source tracks: stereo S16 big-endian at SOURCE_RATE.
VENDORED_TRACKS = (
    ("ode_to_joy", "Ode to Joy - Beethoven"),
    ("minuet_in_g", "Minuet in G - Petzold"),
    ("greensleeves", "Greensleeves - traditional"),
)


def write_header(path, assets):
    lines = [
        "#ifndef LIBSATURN_CD_STREAMING_JUKEBOX_DATA_H",
        "#define LIBSATURN_CD_STREAMING_JUKEBOX_DATA_H",
        "",
        f"#define CD_JUKEBOX_SAMPLE_RATE {RATE}u",
        f"#define CD_JUKEBOX_CHANNELS {CHANNELS}u",
        f"#define CD_JUKEBOX_TRACK_COUNT {len(assets)}u",
        "",
    ]
    for name, _title, sample_count, byte_count in assets:
        macro = name.upper()
        lines.extend((
            f"#define CD_JUKEBOX_{macro}_SAMPLES {sample_count}u",
            f"#define CD_JUKEBOX_{macro}_BYTES {byte_count}u",
            "",
        ))
    lines.extend(("#endif /* LIBSATURN_CD_STREAMING_JUKEBOX_DATA_H */", ""))
    path.write_text("\n".join(lines), encoding="utf-8")


def downsample_2x_s16be_stereo(pcm):
    """Box-filter adjacent stereo frames, preserving big-endian S16."""
    output = bytearray(len(pcm) // 2)
    out = 0
    for offset in range(0, len(pcm), 8):
        for channel in range(CHANNELS):
            a = int.from_bytes(pcm[offset + channel * 2:offset + channel * 2 + 2],
                               "big", signed=True)
            b_offset = offset + 4 + channel * 2
            b = int.from_bytes(pcm[b_offset:b_offset + 2], "big", signed=True)
            sample = (a + b) // 2
            output[out:out + 2] = sample.to_bytes(2, "big", signed=True)
            out += 2
    return bytes(output)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--out-dir", required=True, type=Path)
    parser.add_argument("--out-h", required=True, type=Path)
    parser.add_argument("--vendored-dir", required=True, type=Path,
                        help="dir with vendored big-endian S16 stereo tracks at RATE")
    args = parser.parse_args()

    args.out_dir.mkdir(parents=True, exist_ok=True)
    assets = []
    for name, title in VENDORED_TRACKS:
        pcm = (args.vendored_dir / f"{name}.s16be").read_bytes()
        if len(pcm) == 0 or len(pcm) % (4 * CHANNELS) != 0:
            raise SystemExit(f"invalid vendored PCM: {name}")
        pcm = downsample_2x_s16be_stereo(pcm)
        frames = len(pcm) // (2 * CHANNELS)
        (args.out_dir / f"{name}.pcm").write_bytes(pcm)
        assets.append((name, title, frames, len(pcm)))
        print(f"[cd-streaming-jukebox] {name}: {frames} frames, "
              f"{len(pcm)} bytes (vendored recording)")
    args.out_h.parent.mkdir(parents=True, exist_ok=True)
    write_header(args.out_h, assets)


if __name__ == "__main__":
    main()
