#!/usr/bin/env python3
"""Regression tests for Elecbyte SND extraction and WAV -> Saturn S8 conversion."""

from __future__ import annotations

import io
import struct
import sys
import tempfile
import wave
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))

from tools.ikemen_snd import extract_snd_entry, wav_to_s8_mono  # noqa: E402


def make_wav(samples: bytes, channels: int = 1, width: int = 1, rate: int = 11025) -> bytes:
    out = io.BytesIO()
    with wave.open(out, "wb") as wav:
        wav.setnchannels(channels)
        wav.setsampwidth(width)
        wav.setframerate(rate)
        wav.writeframes(samples)
    return out.getvalue()


wav_a = make_wav(bytes([0, 128, 255]))
wav_b = make_wav(struct.pack("<hhhh", -32768, 32767, -16384, 16384), channels=2, width=2, rate=22050)

header_size = 24
entry_a = header_size
entry_b = entry_a + 16 + len(wav_a)
snd = bytearray()
snd += b"ElecbyteSnd\x00"
snd += struct.pack("<HHII", 1, 0, 2, entry_a)
snd += struct.pack("<IIii", entry_b, len(wav_a), 0, 0)
snd += wav_a
snd += struct.pack("<IIii", 0, len(wav_b), 5, 1)
snd += wav_b

with tempfile.TemporaryDirectory() as td:
    path = Path(td) / "test.snd"
    path.write_bytes(snd)
    assert extract_snd_entry(path, 0, 0) == wav_a
    assert extract_snd_entry(path, 5, 1) == wav_b
    try:
        extract_snd_entry(path, 9, 9)
    except KeyError:
        pass
    else:
        raise AssertionError("missing SND entry was accepted")

pcm_a, rate_a = wav_to_s8_mono(wav_a)
assert rate_a == 11025
assert pcm_a == bytes([128, 0, 127])

pcm_b, rate_b = wav_to_s8_mono(wav_b)
assert rate_b == 22050
# Stereo averages: (-128 + 127) / 2 truncates toward zero -> 0,
# then (-64 + 64) / 2 -> 0.
assert pcm_b == bytes([0, 0])

print("ikemen SND tooling: OK")
