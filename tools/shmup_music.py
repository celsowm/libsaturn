#!/usr/bin/env python3
"""Procedural stage music for examples/bullet_hell_stress: an original chiptune loop.

Writes signed stereo PCM at 22050 Hz, the formats sat_audio_stream plays (see
docs/SCSP_AUDIO_STREAMING_GUIDE.md), and a sizing header. The default is 8-bit: the disc is read
through the CD Block's 16-bit data port by the CPU, which is the dearest part of streaming music, so
the example halves the bytes (dithered, so the quantisation stays noise rather than distortion);
--bits 16 writes big-endian S16. The loop is seamless: every note
tail that would run past the end is folded back onto the start, so the sample after the last one
continues the waveform. Nothing here is taken from a game; the output is deterministic.

    shmup_music.py --out STAGE.PCM --header bh_music.h [--bits 8|16]
"""
from __future__ import annotations

import argparse
import sys
from pathlib import Path

import numpy as np

RATE = 22050
CHANNELS = 2
BPM = 150
STEPS_PER_BAR = 16
BARS = 8
STEP = RATE * 60 // BPM // 4            # samples per sixteenth
FRAMES = STEP * STEPS_PER_BAR * BARS
PEAK = 0.55                              # of full scale: headroom for the mixer

# A minor, F, C, G: two bars each. Semitones above A2 for the bass and the arpeggio chord tones.
PROGRESSION = [(0, (0, 3, 7)), (-4, (0, 4, 7)), (3, (0, 4, 7)), (-2, (0, 4, 7))]


def hz(semitones: float) -> float:
    return 110.0 * 2.0 ** (semitones / 12.0)


def square(phase: np.ndarray, duty: float) -> np.ndarray:
    return np.where((phase % 1.0) < duty, 1.0, -1.0)


class Mix:
    def __init__(self) -> None:
        self.left = np.zeros(FRAMES + RATE, np.float64)       # one second of tail room
        self.right = np.zeros_like(self.left)

    def add(self, start: int, wave: np.ndarray, pan: float) -> None:
        end = min(start + len(wave), len(self.left))
        w = wave[:end - start]
        self.left[start:end] += w * (1.0 - pan) * 0.5 * 2
        self.right[start:end] += w * (1.0 + pan) * 0.5 * 2

    def fold(self) -> tuple[np.ndarray, np.ndarray]:
        out = []
        for ch in (self.left, self.right):
            body = ch[:FRAMES].copy()
            tail = ch[FRAMES:]
            body[:len(tail)] += tail                           # the loop wraps: the tail lands on the start
            out.append(body)
        return out[0], out[1]


def note(freq: float, length: int, kind: str, decay: float) -> np.ndarray:
    t = np.arange(length) / RATE
    phase = t * freq
    env = np.exp(-t * decay) * np.minimum(1.0, t * 400.0)
    if kind == "bass":
        return (0.6 * square(phase, 0.5) + 0.4 * (2.0 * (phase % 1.0) - 1.0)) * env
    return square(phase, 0.25) * env


def drum(kind: str, length: int, rng: np.random.Generator) -> np.ndarray:
    t = np.arange(length) / RATE
    if kind == "kick":
        return np.sin(2 * np.pi * (55.0 * t + 90.0 * (1 - np.exp(-t * 30.0)) / 30.0)) * np.exp(-t * 18.0)
    if kind == "snare":
        return (rng.uniform(-1, 1, length) * 0.8 + np.sin(2 * np.pi * 190.0 * t) * 0.3) * np.exp(-t * 26.0)
    return rng.uniform(-1, 1, length) * np.exp(-t * 90.0) * 0.35        # hat


def render() -> bytes:
    rng = np.random.default_rng(0x5A7)
    mix = Mix()
    for bar in range(BARS):
        root, chord = PROGRESSION[bar // 2]
        for step in range(STEPS_PER_BAR):
            at = (bar * STEPS_PER_BAR + step) * STEP
            if step % 4 == 0:
                mix.add(at, note(hz(root - 12), STEP * 4, "bass", 6.0) * 0.5, 0.0)
            elif step % 4 == 2:
                mix.add(at, note(hz(root - 12 + 12), STEP * 2, "bass", 9.0) * 0.28, 0.0)
            tone = chord[(step * 3 + bar) % 3] + 12 * (1 + (step // 4 + bar) % 2)
            mix.add(at, note(hz(root + tone), STEP * 2, "lead", 11.0) * 0.22, -0.35 if step % 2 else 0.35)
            if step % 8 == 0:
                mix.add(at, drum("kick", STEP * 3, rng) * 0.7, 0.0)
            if step % 8 == 4:
                mix.add(at, drum("snare", STEP * 3, rng) * 0.45, 0.0)
            if step % 2 == 0:
                mix.add(at, drum("hat", STEP, rng), 0.15 if (step // 2) % 2 else -0.15)
    left, right = mix.fold()
    peak = max(np.abs(left).max(), np.abs(right).max())
    scale = PEAK * 32767.0 / peak
    pcm = np.empty(FRAMES * 2, ">i2")
    pcm[0::2] = np.rint(left * scale).astype(np.int16)
    pcm[1::2] = np.rint(right * scale).astype(np.int16)
    return pcm.tobytes()


def to_s8(pcm16: bytes) -> bytes:
    """S16BE stereo -> S8 with triangular dither (deterministic)."""
    x = np.frombuffer(pcm16, ">i2").astype(np.float64) / 256.0
    rng = np.random.default_rng(0xD17)
    x = x + rng.uniform(-0.5, 0.5, x.size) + rng.uniform(-0.5, 0.5, x.size)
    return np.clip(np.rint(x), -128, 127).astype(np.int8).tobytes()


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--out", type=Path, required=True)
    ap.add_argument("--header", type=Path)
    ap.add_argument("--bits", type=int, choices=(8, 16), default=8)
    a = ap.parse_args(argv)
    pcm = render()
    if a.bits == 8:
        pcm = to_s8(pcm)
    a.out.parent.mkdir(parents=True, exist_ok=True)
    a.out.write_bytes(pcm)
    if a.header:
        a.header.parent.mkdir(parents=True, exist_ok=True)
        a.header.write_text("\n".join([
            "/* Generated by tools/shmup_music.py. Do not edit. */",
            "#ifndef BH_MUSIC_H", "#define BH_MUSIC_H", "",
            f"#define BH_MUSIC_RATE {RATE}u", f"#define BH_MUSIC_CHANNELS {CHANNELS}u",
            f"#define BH_MUSIC_FRAMES {FRAMES}u", f"#define BH_MUSIC_SAMPLE_BYTES {a.bits // 8}u",
            f"#define BH_MUSIC_BYTES {len(pcm)}u", "", "#endif", ""]),
            encoding="utf-8")
    print(f"[shmup-music] {a.out.name}: {FRAMES} frames, {FRAMES / RATE:.1f} s, {len(pcm)} bytes")
    return 0


if __name__ == "__main__":
    sys.exit(main())
