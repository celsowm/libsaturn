#!/usr/bin/env python3
from __future__ import annotations

import tempfile
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))

from tools.ikemen_cmd import parse_cmd  # noqa: E402

SOURCE = r"""
[Defaults]
command.time = 15
command.buffer.time = 1

[Command]
name = "QCF_x"
command = ~D, DF, F, x

[Command]
name = "FF"
command = F, F
time = 10

[Command]
name = "holddown"
command = /$D
time = 1

[Command]
name = "Triple"
command = ~D, DF, F, D, DF, F, x
time = 20

[Command]
name = "Triple"
command = ~D, DF, F, D, DF, F, y
time = 20
"""

with tempfile.TemporaryDirectory() as td:
    path = Path(td) / "test.cmd"
    path.write_text(SOURCE, encoding="utf-8")
    asset = parse_cmd(path)

assert asset.names == ["QCF_x", "FF", "holddown", "Triple"]
assert len(asset.patterns) == 5

qcf = asset.patterns[0]
assert [k.name for k in qcf.steps[0].keys] == ["D"]
assert qcf.steps[0].keys[0].tilde
assert [k.name for k in qcf.steps[1].keys] == ["DF"]
assert qcf.loop_order == [2, 3, 0, 1]

ff = asset.patterns[1]
assert len(ff.steps) == 3
assert ff.steps[0].keys[0].name == "F"
assert ff.steps[1].greater and ff.steps[1].keys[0].tilde
assert ff.steps[2].greater and not ff.steps[2].keys[0].tilde
assert ff.time == 10

hold = asset.patterns[2]
assert hold.steps[0].keys[0].slash
assert hold.steps[0].keys[0].dollar
assert hold.buffer == 1

assert sum(1 for p in asset.patterns if p.name == "Triple") == 2

print("ikemen CMD compiler: OK")
