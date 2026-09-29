#!/usr/bin/env python3
"""AIR parser regression: exact Clsn1/Clsn2 defaults and per-frame overrides."""

from __future__ import annotations

import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))

from tools.ikemen_sff.air import parse  # noqa: E402

air = """
[Begin Action 200]
Clsn2Default: 1
Clsn2[0] = -10, 0, 19, -80
0,0,0,0,2
Clsn1: 1
Clsn1[0] = 16,-80,61,-71
Clsn2: 0
0,1,0,0,4
Clsn1Default: 1
Clsn1[0] = 1,2,3,4
0,2,0,0,1
Clsn1: 0
0,3,0,0,1
"""

with tempfile.TemporaryDirectory() as td:
    path = Path(td) / "test.air"
    path.write_text(air, encoding="utf-8")
    action = parse(path)[200]

assert action.frames[0].clsn1 == []
assert action.frames[0].clsn2 == [(-10, 0, 19, -80)]
assert action.frames[1].clsn1 == [(16, -80, 61, -71)]
assert action.frames[1].clsn2 == []
assert action.frames[2].clsn1 == [(1, 2, 3, 4)]
assert action.frames[2].clsn2 == [(-10, 0, 19, -80)]
assert action.frames[3].clsn1 == []
assert action.frames[3].clsn2 == [(-10, 0, 19, -80)]

print("ikemen AIR collision parsing: OK")
