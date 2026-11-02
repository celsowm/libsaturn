#!/usr/bin/env python3
"""Emit the pcm8_stream example's assets as C arrays.

Both are CC0 1.0 from OpenGameArt (see examples/audio_showcase/LICENSES.md) and are downloaded once into
--cache-dir, which the audio_showcase cache satisfies as is:
  * music "8 Bit Disco Loop" (cosmac): one signed 8-bit array per channel, since LibSaturn streams a stereo
    track as two mono streams. The source is unsigned 8-bit, so the conversion loses nothing.
  * effect "Explosion" (TinyWorlds): signed 16-bit mono, played as a resident sound over the music.
There is no fallback: a missing download fails the build.
"""
import argparse
import struct
import urllib.request
import wave
from pathlib import Path

BASE = "https://opengameart.org/sites/default/files/"
MUSIC = ("title.wav", BASE + "title_1.wav")
EFFECT = ("explosion.wav", BASE + "explosion.wav")


def load(cache_dir, asset):
    name, url = asset
    path = Path(cache_dir) / name
    if not path.exists():
        path.parent.mkdir(parents=True, exist_ok=True)
        request = urllib.request.Request(url, headers={"User-Agent": "libsaturn-asset-fetch"})
        with urllib.request.urlopen(request, timeout=30) as response:
            path.write_bytes(response.read())
    with wave.open(str(path), "rb") as wav:
        channels, width, rate = wav.getnchannels(), wav.getsampwidth(), wav.getframerate()
        raw = wav.readframes(wav.getnframes())
    if width not in (1, 2) or channels not in (1, 2):
        raise SystemExit(f"{path}: expected 8/16-bit mono or stereo, got {width * 8}-bit x{channels}")
    if width == 1:                        # unsigned 8-bit WAV: recentre, on the 16-bit scale
        samples = [(b - 128) << 8 for b in raw]
    else:
        samples = struct.unpack(f"<{len(raw) // 2}h", raw)
    return rate, [samples[c::channels] for c in (0, channels - 1)]


def array(kind, name, values, per_line=24):
    lines = [f"const {kind} {name}[{len(values)}] = {{\n"]
    for start in range(0, len(values), per_line):
        lines.append(",".join(str(v) for v in values[start:start + per_line]) + ",\n")
    lines.append("};\n")
    return "".join(lines)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out-c", required=True)
    parser.add_argument("--out-h", required=True)
    parser.add_argument("--cache-dir", required=True)
    args = parser.parse_args()
    music_rate, music = load(args.cache_dir, MUSIC)
    effect_rate, effect = load(args.cache_dir, EFFECT)
    frames, effect_frames = len(music[0]), len(effect[0])
    Path(args.out_h).write_text(
        "#ifndef PCM8_STREAM_DATA_H\n#define PCM8_STREAM_DATA_H\n#include <stdint.h>\n"
        f"#define PCM8_MUSIC_RATE {music_rate}u\n#define PCM8_MUSIC_FRAMES {frames}u\n"
        f"#define PCM8_EFFECT_RATE {effect_rate}u\n#define PCM8_EFFECT_FRAMES {effect_frames}u\n"
        "extern const int8_t pcm8_music_left[PCM8_MUSIC_FRAMES];\n"
        "extern const int8_t pcm8_music_right[PCM8_MUSIC_FRAMES];\n"
        "extern const int16_t pcm8_effect[PCM8_EFFECT_FRAMES];\n#endif\n")
    out = ['#include "pcm8_stream/pcm8_data.h"\n']
    for name, plane in zip(("left", "right"), music):
        out.append(array("int8_t", f"pcm8_music_{name}", [v >> 8 for v in plane]))
    out.append(array("int16_t", "pcm8_effect", effect[0], 16))
    Path(args.out_c).write_text("".join(out))


if __name__ == "__main__":
    main()
