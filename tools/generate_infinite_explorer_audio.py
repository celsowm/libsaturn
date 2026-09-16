#!/usr/bin/env python3
from __future__ import annotations

import argparse
import hashlib
import math
import urllib.error
import urllib.request
import wave
from pathlib import Path

RATE = 11025
MAX_SAMPLES = 65535
DOWNLOAD_TIMEOUT = 15

# Prefer immutable GitHub-hosted mirrors/copies so building LibSaturn does not
# depend on OpenGameArt's file CDN being reachable. Each item remains CC0 and
# its original provenance is documented in examples/infinite_explorer/assets/
# ATTRIBUTION.md. Original source URLs remain as secondary fallbacks where
# useful.
ASSETS = {
    "explorer_engine": {
        "urls": [
            "https://raw.githubusercontent.com/Damerlan/polar-assault/6a4bd8330c06241df27991ae72bed992376c6621/Assets/Audio/music/sfx/rocket_engine.001.wav",
            "https://opengameart.org/sites/default/files/rocket_engine.001.wav",
        ],
        "filename": "rocket_engine.001.wav",
        "seconds": 1.60,
        "loop": True,
        "sha256": "dc82ef86d6af2278d6592d20372737c32620f1c1957290c79f6643af5381f277",
    },
    "explorer_scanner": {
        "urls": [
            "https://raw.githubusercontent.com/euuuuuuan/voidclad-public/440916aabc30abe014cb33ad90bd150bfbf22dd0/assets/sfx/ricochet_ping.wav",
        ],
        "filename": "ricochet_ping.wav",
        "seconds": 0.18,
        "loop": False,
        "sha256": "971a301fa89fd51a7ceeb4d8e948de393af5edd1112cd00e77090aa7624a3556",
    },
    "explorer_fire": {
        "urls": [
            "https://raw.githubusercontent.com/euuuuuuan/voidclad-public/440916aabc30abe014cb33ad90bd150bfbf22dd0/assets/sfx/fire_heavy.wav",
        ],
        "filename": "fire_heavy.wav",
        "seconds": 0.70,
        "loop": False,
        "sha256": "cfbbaeb156bb9992ac571d6dd820fcff2a5702b276c9bb59f12a38f6ad7eadde",
    },
    "explorer_impact": {
        "urls": [
            "https://raw.githubusercontent.com/euuuuuuan/voidclad-public/440916aabc30abe014cb33ad90bd150bfbf22dd0/assets/sfx/impact_pen.wav",
        ],
        "filename": "impact_pen.wav",
        "seconds": 0.85,
        "loop": False,
        "sha256": "935cbabff9aa83176bf946db7a00204def63a4ae7fa83b64f92d6732a63f74f4",
    },
    "explorer_music": {
        "urls": [
            "https://raw.githubusercontent.com/Beatscribe/homebrew_vgm/5a82f88b87bb442499685c494b7a96278121b4c7/SEGA_GEN/fanfares/wav/distant_flute.wav",
        ],
        "filename": "distant_flute.wav",
        "seconds": 4.00,
        "loop": True,
    },
    "explorer_storm": {
        "urls": [
            "https://raw.githubusercontent.com/ExCodeCowboy/StudentGrouper/75cc0b3e101bb6cfb824ac9149b46361e91991d9/public/sounds/reveals/wave.wav",
        ],
        "filename": "wave.wav",
        "seconds": 1.80,
        "loop": True,
        "sha256": "23eed75efff0950354376a7073917839ea80711d9f78724d519fa2a643e9aaa9",
    },
}


def clamp16(v: int) -> int:
    return max(-32768, min(32767, int(v)))


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def validate_file(path: Path, expected_sha256: str | None) -> None:
    data = path.read_bytes()
    if len(data) < 44:
        raise ValueError("downloaded WAV is too small")
    if expected_sha256 is not None:
        actual = sha256_bytes(data)
        if actual.lower() != expected_sha256.lower():
            raise ValueError(f"SHA-256 mismatch: expected {expected_sha256}, got {actual}")


def download_one(url: str) -> bytes:
    req = urllib.request.Request(
        url,
        headers={"User-Agent": "libsaturn-infinite-explorer/1.1"},
    )
    with urllib.request.urlopen(req, timeout=DOWNLOAD_TIMEOUT) as response:
        data = response.read()
    if len(data) < 44:
        raise ValueError("downloaded WAV is too small")
    return data


def ensure_downloaded(spec: dict[str, object], path: Path) -> str:
    expected_sha256 = spec.get("sha256")
    expected = str(expected_sha256) if expected_sha256 is not None else None

    if path.exists():
        try:
            validate_file(path, expected)
            return "cache"
        except (OSError, ValueError):
            path.unlink(missing_ok=True)

    path.parent.mkdir(parents=True, exist_ok=True)
    failures: list[str] = []
    for raw_url in spec["urls"]:
        url = str(raw_url)
        try:
            data = download_one(url)
            if expected is not None:
                actual = sha256_bytes(data)
                if actual.lower() != expected.lower():
                    raise ValueError(f"SHA-256 mismatch: expected {expected}, got {actual}")
            path.write_bytes(data)
            return url
        except (OSError, ValueError, urllib.error.URLError) as exc:
            failures.append(f"{url}: {exc}")

    raise OSError("; ".join(failures))


def read_sample(raw: bytes, offset: int, width: int) -> int:
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


def decode_wav_mono(path: Path) -> tuple[list[int], int]:
    with wave.open(str(path), "rb") as wav:
        if wav.getcomptype() != "NONE":
            raise ValueError("compressed WAV is not supported")
        channels = wav.getnchannels()
        width = wav.getsampwidth()
        rate = wav.getframerate()
        frames = wav.getnframes()
        raw = wav.readframes(frames)
    stride = channels * width
    if channels <= 0 or rate <= 0 or frames <= 0 or len(raw) < frames * stride:
        raise ValueError("invalid or truncated WAV")
    out: list[int] = []
    for frame in range(frames):
        base = frame * stride
        total = 0
        for channel in range(channels):
            total += read_sample(raw, base + channel * width, width)
        out.append(clamp16(total // channels))
    return out, rate


def resample(samples: list[int], source_rate: int, seconds: float) -> list[int]:
    target_count = min(
        MAX_SAMPLES,
        int(RATE * seconds),
        max(1, (len(samples) * RATE) // source_rate),
    )
    out: list[int] = []
    for i in range(target_count):
        pos_num = i * source_rate
        idx = pos_num // RATE
        frac = pos_num % RATE
        if idx >= len(samples) - 1:
            out.append(samples[-1])
        else:
            a = samples[idx]
            b = samples[idx + 1]
            out.append(clamp16((a * (RATE - frac) + b * frac) // RATE))
    return out


def make_loop_friendly(samples: list[int]) -> list[int]:
    if len(samples) < 128:
        return samples
    n = min(384, len(samples) // 6)
    out = samples[:]
    for i in range(n):
        a = out[i]
        b = out[len(out) - n + i]
        mixed = (a * i + b * (n - i)) // n
        out[i] = mixed
        out[len(out) - n + i] = mixed
    return out


def fallback(name: str, seconds: float, loop: bool) -> list[int]:
    count = max(1, int(RATE * seconds))
    out: list[int] = []
    seed = sum(ord(c) for c in name) | 1
    for i in range(count):
        t = i / RATE
        if "engine" in name:
            value = math.sin(2 * math.pi * 83 * t) * 10000 + math.sin(2 * math.pi * 127 * t) * 5000
        elif "storm" in name:
            seed ^= (seed << 13) & 0xFFFFFFFF
            seed ^= seed >> 17
            seed ^= (seed << 5) & 0xFFFFFFFF
            value = (((seed & 0xFFFF) - 32768) / 32768.0) * 9000
        elif "music" in name:
            notes = (164.81, 220.0, 246.94, 329.63)
            f = notes[(int(t * 4.0) // 2) % len(notes)]
            value = math.sin(2 * math.pi * f * t) * 7000 + math.sin(2 * math.pi * f * 0.5 * t) * 3500
        elif "impact" in name:
            env = max(0.0, 1.0 - i / max(1, count))
            seed = (seed * 1664525 + 1013904223) & 0xFFFFFFFF
            value = (((seed >> 8) & 0xFFFF) - 32768) * env * 0.5
        elif "fire" in name:
            env = max(0.0, 1.0 - i / max(1, count))
            f = 1400.0 - 1000.0 * i / max(1, count)
            value = math.sin(2 * math.pi * f * t) * 12000 * env
        else:
            env = 1.0 if loop else max(0.0, 1.0 - i / max(1, count))
            value = math.sin(2 * math.pi * 1200.0 * t) * 11000 * env
        out.append(clamp16(value))
    return make_loop_friendly(out) if loop else out


def to_s8(samples: list[int]) -> list[int]:
    return [max(-128, min(127, sample >> 8)) for sample in samples]


def emit_array(name: str, values: list[int]) -> str:
    lines = [f"const int8_t {name}[] = {{"]
    for i in range(0, len(values), 16):
        lines.append("    " + ", ".join(str(v) for v in values[i:i + 16]) + ",")
    lines.append("};")
    lines.append(f"const uint32_t {name}_count = {len(values)}u;")
    return "\n".join(lines)


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--out-c", required=True)
    ap.add_argument("--out-h", required=True)
    ap.add_argument("--cache-dir")
    ap.add_argument("--require-real", action="store_true")
    args = ap.parse_args()

    out_c = Path(args.out_c)
    out_h = Path(args.out_h)
    cache_dir = Path(args.cache_dir) if args.cache_dir else out_c.parent / "cc0_cache"

    converted: dict[str, list[int]] = {}
    real_count = 0
    for name, spec in ASSETS.items():
        path = cache_dir / str(spec["filename"])
        try:
            source = ensure_downloaded(spec, path)
            if source != "cache":
                print(f"[infinite-explorer] downloaded {spec['filename']} from GitHub mirror")
            else:
                print(f"[infinite-explorer] cached {spec['filename']}")
            samples, source_rate = decode_wav_mono(path)
            samples = resample(samples, source_rate, float(spec["seconds"]))
            if bool(spec["loop"]):
                samples = make_loop_friendly(samples)
            if not samples:
                raise ValueError("empty converted audio")
            real_count += 1
        except (OSError, ValueError, wave.Error, urllib.error.URLError) as exc:
            if args.require_real:
                raise SystemExit(f"failed to fetch/decode {name}: {exc}") from exc
            print(f"[infinite-explorer] warning: {name}: {exc}; using deterministic fallback")
            samples = fallback(name, float(spec["seconds"]), bool(spec["loop"]))
        converted[name] = to_s8(samples)

    out_h.parent.mkdir(parents=True, exist_ok=True)
    header = [
        "#ifndef INFINITE_EXPLORER_AUDIO_DATA_H",
        "#define INFINITE_EXPLORER_AUDIO_DATA_H",
        "",
        "#include <stdint.h>",
        "",
        f"#define EXPLORER_AUDIO_SAMPLE_RATE {RATE}u",
        f"#define EXPLORER_REAL_AUDIO_COUNT {real_count}u",
        f"#define EXPLORER_REAL_AUDIO_TOTAL {len(ASSETS)}u",
        "",
    ]
    source = ['#include "audio_data.h"', ""]
    for name, values in converted.items():
        header += [f"extern const int8_t {name}[];", f"extern const uint32_t {name}_count;"]
        source += [emit_array(name, values), ""]
    header += ["", "#endif", ""]

    out_h.write_text("\n".join(header), encoding="utf-8")
    out_c.write_text("\n".join(source), encoding="utf-8")


if __name__ == "__main__":
    main()
