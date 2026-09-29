#!/usr/bin/env python3
"""Regression test for the SFF v2 LZ5 rebuilt-distance bit packing."""

from __future__ import annotations

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))

from tools.ikemen_sff.codecs import _decode_lz5  # noqa: E402


# One control group:
#   - four 64-pixel literal runs (colors 1, 2, 3, 4)
#   - four short back-references
#
# The short references contribute their top two bits to a rebuilt distance
# byte. With 0xC1 each time the correct accumulation is:
#   C0 | 30 | 0C | 03 = FF
# so the fourth reference uses distance 0xFF + 1 = 256 and copies color 1.
# The old Python port parsed Ikemen GO's expression as
#   d & (0xC0 >> rbc)
# instead of
#   (d & 0xC0) >> rbc
# which reconstructed 0xC1 (distance 194) and copied color 2 instead.
stream = bytes([
    0, 0, 0, 0,             # SFF v2 compressed-data prefix
    0xF0,                    # four literals, then four back-references
    1, 56,                   # 64 x color 1
    2, 56,                   # 64 x color 2
    3, 56,                   # 64 x color 3
    4, 56,                   # 64 x color 4
    0xC1, 0,                 # short ref, explicit distance 1
    0xC1, 0,                 # short ref, explicit distance 1
    0xC1, 0,                 # short ref, explicit distance 1
    0xC1,                    # short ref, rebuilt distance 256
    0,                       # decoder's next control-byte read
])

decoded = _decode_lz5(stream, 264, 1)

assert decoded[0:64] == bytes([1]) * 64
assert decoded[64:128] == bytes([2]) * 64
assert decoded[128:192] == bytes([3]) * 64
assert decoded[192:256] == bytes([4]) * 64
assert decoded[256:262] == bytes([4]) * 6
assert decoded[262:264] == bytes([1]) * 2

print("ikemen SFF LZ5 codec: OK")
