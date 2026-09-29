#!/usr/bin/env python3
from __future__ import annotations

import tempfile
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))

from tools.ikemen_state_rules import parse_state_rules  # noqa: E402

SOURCE = r"""
[Command]
name = "x"
command = x
[Command]
name = "y"
command = y
[Command]
name = "holddown"
command = /$D
time = 1

[Statedef -1]

[State -1, Stand Light Punch]
type = ChangeState
value = 200
triggerall = command = "x"
triggerall = command != "holddown"
trigger1 = statetype = S
trigger1 = ctrl
trigger2 = stateno = 200
trigger2 = time > 6

[State -1, Jump Strong Punch]
type = ChangeState
value = 610
triggerall = command = "y"
trigger1 = statetype = A
trigger1 = ctrl
trigger2 = stateno = 600 || stateno = 630
trigger2 = movecontact
"""

with tempfile.TemporaryDirectory() as td:
    path = Path(td) / "test.cmd"
    path.write_text(SOURCE, encoding="utf-8")
    rules, diagnostics = parse_state_rules(path, {200, 610})

assert diagnostics == []
assert [r.target for r in rules] == [200, 610]
ops200 = [i.op for i in rules[0].code]
assert ops200.count("command_active") == 1
assert ops200.count("command_inactive") == 1
assert "state_type_eq" in ops200
assert "state_time_gt" in ops200
assert ops200[-1] == "and"
ops610 = [i.op for i in rules[1].code]
assert ops610.count("state_no_eq") == 2
assert "move_contact" in ops610
assert ops610.count("or") >= 2
print("ikemen state-rule compiler: OK")
