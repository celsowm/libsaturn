#!/usr/bin/env python3
"""Stage the CD-only PCM tracks for cd_streaming_jukebox.

All three tracks are freely licensed renders from Wikimedia, vendored as
build-ready big-endian S16 stereo @ 44100 Hz (CD quality; see
examples/cd_streaming_jukebox/audio-src/ and ../LICENSES.md).  This tool just
copies them into the ISO staging dir and emits the sizing header, so the
example build needs no network access and no audio decoder.
"""

import argparse
from pathlib import Path

RATE = 44100
CHANNELS = 2
# Vendored build-ready tracks: mono S16 big-endian at RATE.
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
        if len(pcm) == 0 or len(pcm) % (2 * CHANNELS) != 0:
            raise SystemExit(f"invalid vendored PCM: {name}")
        frames = len(pcm) // (2 * CHANNELS)
        (args.out_dir / f"{name}.pcm").write_bytes(pcm)
        assets.append((name, title, frames, len(pcm)))
        print(f"[cd-streaming-jukebox] {name}: {frames} frames, "
              f"{len(pcm)} bytes (vendored recording)")
    args.out_h.parent.mkdir(parents=True, exist_ok=True)
    write_header(args.out_h, assets)


if __name__ == "__main__":
    main()
