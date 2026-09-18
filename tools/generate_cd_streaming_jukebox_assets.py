#!/usr/bin/env python3
"""Generate the CD-only PCM tracks for cd_streaming_jukebox.

The notes below are short, newly synthesized monophonic arrangements of public
domain compositions.  No recording, sample, score scan, or third-party audio
asset is downloaded or embedded by this tool.
"""

import argparse
import math
import struct
from pathlib import Path

RATE = 11025
NOTE_SECONDS = 0.20

# Semitone offsets from A4.  The source/provenance of each melody is recorded
# in examples/cd_streaming_jukebox/LICENSES.md.
TRACKS = (
    (
        "ode_to_joy",
        "Ode to Joy - Beethoven",
        (2, 2, 3, 5, 5, 3, 2, 0, -2, -2, 0, 2, 2, 0, 0,
         2, 2, 3, 5, 5, 3, 2, 0, -2, -2, 0, 2, 0, -2, -2),
    ),
    (
        "minuet_in_g",
        "Minuet in G - Petzold",
        (-2, -7, -5, -3, -2, -7, -7,
         -5, -7, -5, -3, -5, -2, -2,
         -2, -7, -5, -3, -2, -7, -7,
         -5, -2, 0, 2, -5, -3, -2),
    ),
    (
        "greensleeves",
        "Greensleeves - traditional",
        (-5, -2, 0, 2, 3, 5, 0, -2,
         -3, -5, -7, -3, -5, -2, 0, 2,
         3, 5, 0, -2, -3, -5, -7, -5,
         -3, -2, -5, -7, -9, -5),
    ),
)


def clamp16(value):
    return max(-32768, min(32767, int(value)))


def synthesize(notes):
    samples = []
    count = int(RATE * NOTE_SECONDS)
    attack = max(1, RATE // 100)
    release = max(1, RATE // 50)
    for semitone in notes:
        if semitone is None:
            samples.extend([0] * count)
            continue
        frequency = 440.0 * (2.0 ** (semitone / 12.0))
        for index in range(count):
            envelope = 1.0
            if index < attack:
                envelope = index / attack
            elif index >= count - release:
                envelope = (count - index - 1) / release
            t = index / RATE
            # Deliberately simple, repository-authored timbre: fundamental,
            # octave, and a quiet fifth.  It is not derived from a recording.
            sample = (
                math.sin(2.0 * math.pi * frequency * t) * 10500.0
                + math.sin(2.0 * math.pi * frequency * 2.0 * t) * 3600.0
                + math.sin(2.0 * math.pi * frequency * 1.5 * t) * 1800.0
            ) * envelope
            samples.append(clamp16(sample))
    return samples


def write_header(path, assets):
    lines = [
        "#ifndef LIBSATURN_CD_STREAMING_JUKEBOX_DATA_H",
        "#define LIBSATURN_CD_STREAMING_JUKEBOX_DATA_H",
        "",
        f"#define CD_JUKEBOX_SAMPLE_RATE {RATE}u",
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
    args = parser.parse_args()

    args.out_dir.mkdir(parents=True, exist_ok=True)
    assets = []
    for name, title, notes in TRACKS:
        samples = synthesize(notes)
        # The SH-2 is big-endian and SCSP uploads preserve each source byte pair.
        # Store signed 16-bit PCM in that order so the CD asset matches native
        # int16_t sample data uploaded by the runtime.
        pcm = b"".join(struct.pack(">h", sample) for sample in samples)
        (args.out_dir / f"{name}.pcm").write_bytes(pcm)
        assets.append((name, title, len(samples), len(pcm)))
        print(f"[cd-streaming-jukebox] {name}: {len(samples)} samples, {len(pcm)} bytes")
    args.out_h.parent.mkdir(parents=True, exist_ok=True)
    write_header(args.out_h, assets)


if __name__ == "__main__":
    main()
