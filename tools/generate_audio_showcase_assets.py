#!/usr/bin/env python3
import argparse
import math
from pathlib import Path

RATE = 22050


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


def noise(seconds, amplitude=13000):
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


def loop_wave(freq=110.0, seconds=0.20, amplitude=9000):
    n = int(RATE * seconds)
    cycles = max(1, round(freq * seconds))
    exact = cycles / seconds
    return [clamp16(math.sin(2.0 * math.pi * exact * i / RATE) * amplitude) for i in range(n)]


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
    args = ap.parse_args()

    assets = {
        "showcase_ping": tone(880.0, 0.16),
        "showcase_bass": tone(220.0, 0.24, 19000),
        "showcase_noise": noise(0.18),
        "showcase_sweep": sweep(300.0, 1800.0, 0.28),
        "showcase_loop": loop_wave(),
    }

    h = [
        "#ifndef LIBSATURN_AUDIO_SHOWCASE_DATA_H",
        "#define LIBSATURN_AUDIO_SHOWCASE_DATA_H",
        "#include <stdint.h>",
        f"#define SHOWCASE_SAMPLE_RATE {RATE}u",
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
