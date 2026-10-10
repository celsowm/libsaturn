#!/usr/bin/env python3
"""tools/satlz.py: round trips, and the in-place layout sat_load_lz decodes with (saturn/lz.h)."""

from __future__ import annotations

import random
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))

import satlz  # noqa: E402


def decode_in_place(blob: bytes, capacity: int) -> bytes:
    """sat_load_lz on one bytearray: the file at align_down(capacity - size, 4), decoded from 0.
    Every write that lands on a payload byte not yet read is an error."""
    decoded, _margin, size = struct.unpack(">III", blob[4:16])
    buf = bytearray(capacity)
    start = (capacity - len(blob)) & ~3
    buf[start:start + len(blob)] = blob
    i = start + satlz.HEADER  # next unread payload byte
    end = i + size
    o = 0

    def put(value: int) -> None:
        nonlocal o
        assert o < i, f"write at {o} overtook the input at {i}"
        buf[o] = value
        o += 1

    def length(n: int) -> int:
        nonlocal i
        if n == 15:
            while True:
                b = buf[i]
                i += 1
                n += b
                if b != 255:
                    break
        return n

    while True:
        t = buf[i]
        i += 1
        for _ in range(length(t >> 4)):
            v = buf[i]
            i += 1
            put(v)
        if i >= end:
            break
        off = (buf[i] << 8) | buf[i + 1]
        i += 2
        for _ in range(length(t & 15) + satlz.MIN_MATCH):
            put(buf[o - off])
    assert o == decoded
    return bytes(buf[:decoded])


def samples() -> list[bytes]:
    rng = random.Random(1)
    tiles = bytes(rng.choice([0, 0, 0, 1, 2, 3, 0x11, 0x22]) for _ in range(20000))
    return [
        b"",
        b"x",
        b"abcd",
        b"\0" * 70000,                                  # one long run, offset 1
        bytes(rng.randrange(256) for _ in range(5000)),  # incompressible: worst margin
        b"Saturn " * 3000,
        tiles,
        bytes(range(256)) * 300,                        # repeats beyond a 64 KiB window
        bytes(rng.randrange(256) for _ in range(300)) + b"\0" * 5000 + bytes(rng.randrange(4) for _ in range(3000)),
    ]


def main() -> int:
    for data in samples():
        blob = satlz.compress(data)
        assert blob[:4] == b"SLZ1"
        decoded, margin, size = struct.unpack(">III", blob[4:16])
        assert decoded == len(data) and size == len(blob) - satlz.HEADER
        assert satlz.decompress(blob) == data
        # the header's margin is enough at every alignment of the capacity
        for extra in range(4):
            assert decode_in_place(blob, len(data) + margin + extra) == data, (len(data), extra)
    # a bad file is refused, not decoded into garbage
    blob = satlz.compress(b"Saturn " * 100)
    for broken in (b"SLZ2" + blob[4:], blob[:-3]):
        try:
            satlz.decompress(broken)
        except (ValueError, IndexError):
            continue
        raise AssertionError("a broken file decoded")
    print("satlz: OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
