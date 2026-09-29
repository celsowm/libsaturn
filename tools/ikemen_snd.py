#!/usr/bin/env python3
"""Extract selected Elecbyte SND entries and emit Saturn-friendly C PCM tables."""

from __future__ import annotations

import argparse
import io
import re
import struct
import wave
from dataclasses import dataclass
from pathlib import Path


@dataclass(frozen=True)
class PcmAsset:
    name: str
    samples: bytes
    sample_rate: int


def _read_exact(data: bytes, offset: int, size: int) -> bytes:
    end = offset + size
    if offset < 0 or end > len(data):
        raise ValueError("SND offset outside file")
    return data[offset:end]


def extract_snd_entry(path: Path, group: int, number: int) -> bytes:
    data = path.read_bytes()
    if len(data) < 24 or data[:12] != b"ElecbyteSnd\x00":
        raise ValueError(f"{path}: invalid Elecbyte SND header")

    _, _, count, offset = struct.unpack_from("<HHII", data, 12)
    seen: set[int] = set()
    for _ in range(count):
        if offset in seen:
            raise ValueError(f"{path}: cyclic SND subheader chain")
        seen.add(offset)
        header = _read_exact(data, offset, 16)
        next_offset, wav_size, entry_group, entry_number = struct.unpack("<IIii", header)
        if entry_group == group and entry_number == number:
            return _read_exact(data, offset + 16, wav_size)
        if next_offset == 0:
            break
        offset = next_offset
    raise KeyError(f"{path}: sound ({group},{number}) not found")


def wav_to_s8_mono(wav_data: bytes) -> tuple[bytes, int]:
    with wave.open(io.BytesIO(wav_data), "rb") as wav:
        if wav.getcomptype() != "NONE":
            raise ValueError(f"unsupported WAV compression {wav.getcomptype()}")
        channels = wav.getnchannels()
        width = wav.getsampwidth()
        rate = wav.getframerate()
        frames = wav.readframes(wav.getnframes())

    if channels <= 0 or width not in (1, 2, 3, 4):
        raise ValueError(f"unsupported WAV layout: channels={channels}, width={width}")

    frame_bytes = channels * width
    if len(frames) % frame_bytes:
        raise ValueError("truncated WAV frame data")

    out = bytearray()
    for base in range(0, len(frames), frame_bytes):
        total = 0
        for channel in range(channels):
            start = base + channel * width
            raw = frames[start:start + width]
            if width == 1:
                sample = raw[0] - 128
            else:
                sample = int.from_bytes(raw, "little", signed=True) >> (8 * (width - 1))
            total += sample
        sample = int(total / channels)
        sample = max(-128, min(127, sample))
        out.append(sample & 0xFF)
    return bytes(out), rate


def _ident(text: str) -> str:
    value = re.sub(r"[^A-Za-z0-9_]", "_", text)
    if not value or value[0].isdigit():
        value = "_" + value
    return value


def _emit_bytes(name: str, data: bytes) -> str:
    values = [str(b if b < 128 else b - 256) for b in data]
    rows = []
    for i in range(0, len(values), 16):
        rows.append("    " + ", ".join(values[i:i + 16]))
    body = ",\n".join(rows)
    return f"static const int8_t {name}[] = {{\n{body}\n}};\n"


def emit(out_prefix: Path, symbol: str, assets: list[PcmAsset]) -> None:
    sym = _ident(symbol)
    guard = f"{sym.upper()}_H"
    h = [
        "#pragma once",
        "#include <stdint.h>",
        "",
        "typedef struct ik_snd_pcm {",
        "    const int8_t* samples;",
        "    uint32_t sample_count;",
        "    uint32_t sample_rate;",
        "} ik_snd_pcm_t;",
        "",
    ]
    c = [f'#include "{out_prefix.name}.h"', ""]
    for asset in assets:
        key = _ident(asset.name)
        array_name = f"{sym}_{key}_samples"
        object_name = f"{sym}_{key}"
        c.append(_emit_bytes(array_name, asset.samples))
        c.append(
            f"const ik_snd_pcm_t {object_name} = "
            f"{{{array_name}, {len(asset.samples)}u, {asset.sample_rate}u}};\n"
        )
        h.append(f"extern const ik_snd_pcm_t {object_name};")
    h.append("")

    out_prefix.parent.mkdir(parents=True, exist_ok=True)
    out_prefix.with_suffix(".h").write_text("\n".join(h), encoding="utf-8")
    out_prefix.with_suffix(".c").write_text("\n".join(c), encoding="utf-8")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--out-prefix", required=True)
    parser.add_argument("--symbol", required=True)
    parser.add_argument(
        "--sound", action="append", nargs=4,
        metavar=("NAME", "SND", "GROUP", "NUMBER"), required=True,
    )
    args = parser.parse_args(argv)

    assets: list[PcmAsset] = []
    for name, snd_path, group, number in args.sound:
        wav_data = extract_snd_entry(Path(snd_path), int(group), int(number))
        samples, rate = wav_to_s8_mono(wav_data)
        assets.append(PcmAsset(name=name, samples=samples, sample_rate=rate))

    emit(Path(args.out_prefix), args.symbol, assets)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
