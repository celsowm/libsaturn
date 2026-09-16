#!/usr/bin/env python3
import argparse
import math
import urllib.error
import urllib.request
import wave
from pathlib import Path

RATE = 11025
MAX_SAMPLES = 65535

CC0_ASSETS = {
    "showcase_click": {
        "url": "https://opengameart.org/sites/default/files/click.wav",
        "filename": "click.wav",
    },
    "showcase_laser": {
        "url": "https://opengameart.org/sites/default/files/laserthing.wav",
        "filename": "laserthing.wav",
    },
    "showcase_explosion": {
        "url": "https://opengameart.org/sites/default/files/explosion.wav",
        "filename": "explosion.wav",
    },
    "showcase_music": {
        "url": "https://opengameart.org/sites/default/files/title_1.wav",
        "filename": "title.wav",
    },
}


def clamp16(v):
    return max(-32768, min(32767, int(v)))


def tone(freq, seconds, amplitude=18000, decay=True):
    n = int(RATE * seconds)
    out = []
    for i in range(n):
        t = i / RATE
        env = (1.0 - i / n) if decay else 1.0
        out.append(clamp16(math.sin(2.0 * math.pi * freq * t) * amplitude * env))
    return out


def sweep(f0, f1, seconds, amplitude=16000):
    n = int(RATE * seconds)
    out = []
    phase = 0.0
    for i in range(n):
        u = i / max(1, n - 1)
        f = f0 + (f1 - f0) * u
        phase += 2.0 * math.pi * f / RATE
        env = 1.0 - u
        out.append(clamp16(math.sin(phase) * amplitude * env))
    return out


def noise(seconds, amplitude=15000):
    n = int(RATE * seconds)
    x = 0x12345678
    out = []
    for i in range(n):
        x ^= (x << 13) & 0xFFFFFFFF
        x ^= x >> 17
        x ^= (x << 5) & 0xFFFFFFFF
        signed = ((x & 0xFFFF) - 32768) / 32768.0
        env = 1.0 - i / n
        out.append(clamp16(signed * amplitude * env))
    return out


def music_fallback(seconds=3.8):
    n = int(RATE * seconds)
    out = []
    notes = (220.0, 277.18, 329.63, 440.0)
    for i in range(n):
        t = i / RATE
        beat = int(t * 4.0) % len(notes)
        f = notes[beat]
        v = (
            math.sin(2.0 * math.pi * f * t) * 6500
            + math.sin(2.0 * math.pi * (f / 2.0) * t) * 3500
        )
        out.append(clamp16(v))
    return out


def download(url, path):
    path.parent.mkdir(parents=True, exist_ok=True)
    request = urllib.request.Request(
        url,
        headers={"User-Agent": "libsaturn-audio-showcase/1.0"},
    )
    with urllib.request.urlopen(request, timeout=30) as response:
        data = response.read()
    if len(data) < 44:
        raise ValueError("downloaded WAV is too small")
    path.write_bytes(data)


def read_sample(raw, offset, width):
    if width == 1:
        return (raw[offset] - 128) << 8
    if width == 2:
        return int.from_bytes(raw[offset:offset + 2], "little", signed=True)
    if width == 3:
        value = raw[offset] | (raw[offset + 1] << 8) | (raw[offset + 2] << 16)
        if value & 0x800000:
            value -= 1 << 24
        return value >> 8
    if width == 4:
        return int.from_bytes(raw[offset:offset + 4], "little", signed=True) >> 16
    raise ValueError(f"unsupported WAV sample width: {width}")


def decode_wav_mono(path):
    with wave.open(str(path), "rb") as wav:
        if wav.getcomptype() != "NONE":
            raise ValueError("compressed WAV is not supported")
        channels = wav.getnchannels()
        width = wav.getsampwidth()
        rate = wav.getframerate()
        frames = wav.getnframes()
        if channels <= 0 or rate <= 0 or frames <= 0:
            raise ValueError("invalid WAV metadata")
        raw = wav.readframes(frames)

    stride = channels * width
    expected = frames * stride
    if len(raw) < expected:
        raise ValueError("truncated WAV payload")

    mono = []
    for frame in range(frames):
        base = frame * stride
        total = 0
        for channel in range(channels):
            total += read_sample(raw, base + channel * width, width)
        mono.append(clamp16(total // channels))
    return mono, rate


def resample_linear(samples, source_rate, target_rate):
    if source_rate == target_rate:
        return samples[:MAX_SAMPLES]
    count = min(MAX_SAMPLES, max(1, (len(samples) * target_rate) // source_rate))
    out = []
    for i in range(count):
        pos_num = i * source_rate
        idx = pos_num // target_rate
        frac = pos_num % target_rate
        if idx >= len(samples) - 1:
            out.append(samples[-1])
            continue
        a = samples[idx]
        b = samples[idx + 1]
        value = (a * (target_rate - frac) + b * frac) // target_rate
        out.append(clamp16(value))
    return out


def load_cc0_asset(spec, cache_dir):
    path = cache_dir / spec["filename"]
    if not path.exists():
        download(spec["url"], path)
        print(f"[audio-showcase] downloaded {spec['filename']}")
    samples, source_rate = decode_wav_mono(path)
    converted = resample_linear(samples, source_rate, RATE)
    if not converted:
        raise ValueError("empty converted audio")
    return converted


def emit_array(name, values):
    lines = [f"const int16_t {name}[] = {{"]
    for i in range(0, len(values), 12):
        lines.append("    " + ", ".join(str(v) for v in values[i:i + 12]) + ",")
    lines.append("};")
    lines.append(f"const uint32_t {name}_count = {len(values)}u;")
    return "\n".join(lines)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out-c", required=True)
    ap.add_argument("--out-h", required=True)
    ap.add_argument("--cache-dir")
    ap.add_argument("--require-real", action="store_true")
    args = ap.parse_args()

    cache_dir = Path(args.cache_dir) if args.cache_dir else Path(args.out_c).parent / "cc0_cache"

    fallbacks = {
        "showcase_click": tone(1400.0, 0.045, 12000),
        "showcase_laser": sweep(1600.0, 320.0, 0.22),
        "showcase_explosion": noise(0.35),
        "showcase_music": music_fallback(),
    }

    assets = {}
    real_count = 0
    for name, spec in CC0_ASSETS.items():
        try:
            assets[name] = load_cc0_asset(spec, cache_dir)
            real_count += 1
        except (OSError, ValueError, wave.Error, urllib.error.URLError) as exc:
            if args.require_real:
                raise SystemExit(f"failed to fetch/decode {name}: {exc}") from exc
            print(f"[audio-showcase] warning: {name}: {exc}; using synthetic fallback")
            assets[name] = fallbacks[name]

    assets["showcase_reference"] = tone(880.0, 0.16)

    h = [
        "#ifndef LIBSATURN_AUDIO_SHOWCASE_DATA_H",
        "#define LIBSATURN_AUDIO_SHOWCASE_DATA_H",
        "#include <stdint.h>",
        f"#define SHOWCASE_SAMPLE_RATE {RATE}u",
        f"#define SHOWCASE_REAL_ASSET_COUNT {real_count}u",
        f"#define SHOWCASE_REAL_ASSET_TOTAL {len(CC0_ASSETS)}u",
        "",
    ]
    c = [
        '#include "audio_data.h"',
        "",
    ]
    for name, values in assets.items():
        h += [f"extern const int16_t {name}[];", f"extern const uint32_t {name}_count;"]
        c += [emit_array(name, values), ""]
    h += ["", "#endif", ""]

    Path(args.out_h).write_text("\n".join(h), encoding="utf-8")
    Path(args.out_c).write_text("\n".join(c), encoding="utf-8")


if __name__ == "__main__":
    main()
