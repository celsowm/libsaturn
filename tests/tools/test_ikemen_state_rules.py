#!/usr/bin/env python3
from __future__ import annotations

import tempfile
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))

from tools.ikemen_state_rules import emit, parse_state_rules  # noqa: E402

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
[Command]
name = "holdfwd"
command = /$F
time = 1
[Command]
name = "holdback"
command = /$B
time = 1
[Command]
name = "QCF_x"
command = x
[Command]
name = "QCF_y"
command = y
[Command]
name = "QCF_xy"
command = x+y
[Command]
name = "upper_x"
command = x
[Command]
name = "upper_y"
command = y
[Command]
name = "upper_xy"
command = x+y
[Command]
name = "QCB_x"
command = x
[Command]
name = "QCB_y"
command = y
[Command]
name = "QCB_xy"
command = x+y

[Statedef -1]

[State -1, Combo condition Reset]
type = VarSet
trigger1 = 1
var(1) = 0

[State -1, Combo condition Check]
type = VarSet
trigger1 = statetype != A
trigger1 = ctrl
trigger2 = (stateno = [200,299]) || (stateno = [400,499])
trigger2 = stateno != 440
trigger2 = movecontact
trigger3 = stateno = 1310 || stateno = 1330
var(1) = 1

[State -1, Fast Kung Fu Palm]
type = ChangeState
value = 1020
triggerall = command = "QCF_xy"
triggerall = power >= 330
trigger1 = var(1)

[State -1, Light Kung Fu Palm]
type = ChangeState
value = 1000
triggerall = command = "QCF_x"
trigger1 = var(1)

[State -1, Strong Kung Fu Palm]
type = ChangeState
value = 1010
triggerall = command = "QCF_y"
trigger1 = var(1)

[State -1, Fast Kung Fu Upper]
type = ChangeState
value = 1120
triggerall = command = "upper_xy"
triggerall = power >= 330
trigger1 = var(1)

[State -1, Light Kung Fu Upper]
type = ChangeState
value = 1100
triggerall = command = "upper_x"
trigger1 = var(1)

[State -1, Strong Kung Fu Upper]
type = ChangeState
value = 1110
triggerall = command = "upper_y"
trigger1 = var(1)

[State -1, Fast Kung Fu Blow]
type = ChangeState
value = 1220
triggerall = command = "QCB_xy"
triggerall = power >= 330
trigger1 = var(1)

[State -1, Light Kung Fu Blow]
type = ChangeState
value = 1200
triggerall = command = "QCB_x"
trigger1 = var(1)

[State -1, Strong Kung Fu Blow]
type = ChangeState
value = 1210
triggerall = command = "QCB_y"
trigger1 = var(1)

[State -1, Stand Light Punch]
type = ChangeState
value = 200
triggerall = command = "x"
triggerall = command != "holddown"
trigger1 = statetype = S
trigger1 = ctrl
trigger2 = stateno = 200
trigger2 = time > 6

[State -1, Kung Fu Throw]
type = ChangeState
value = 800
triggerall = command = "y"
triggerall = statetype = S
triggerall = ctrl
triggerall = stateno != 100
trigger1 = command = "holdfwd"
trigger1 = p2bodydist X < 3
trigger1 = (p2statetype = S) || (p2statetype = C)
trigger1 = p2movetype != H
trigger2 = command = "holdback"
trigger2 = p2bodydist X < 5
trigger2 = (p2statetype = S) || (p2statetype = C)
trigger2 = p2movetype != H

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
    rules, diagnostics = parse_state_rules(path, {200, 610, 800, 1000, 1010, 1020, 1100, 1110, 1120, 1200, 1210, 1220})
    out_prefix = Path(td) / "generated_rules"
    emit(rules, diagnostics, out_prefix, "test")
    emitted_c = out_prefix.with_suffix(".c").read_text(encoding="utf-8")
    emitted_h = out_prefix.with_suffix(".h").read_text(encoding="utf-8")
    assert "IK_EXPR_LOAD_FIELD" in emitted_c
    assert "IK_EXPR_LOAD_COMMAND" in emitted_c
    assert "IK_EXPR_GE" in emitted_c
    assert "IK_CMD_RULE_" not in emitted_c
    assert "STATE_RULE_INSTRUCTION_COUNT" in emitted_h

assert diagnostics == []
assert [r.target for r in rules] == [1020, 1000, 1010, 1120, 1100, 1110, 1220, 1200, 1210, 200, 800, 610]
ops200 = [i.op for i in rules[9].code]
assert ops200.count("command_active") == 1
assert ops200.count("command_inactive") == 1
assert "state_type_eq" in ops200
assert "state_time_gt" in ops200
assert ops200[-1] == "and"
ops1020 = [i.op for i in rules[0].code]
assert "power_ge" in ops1020
assert "ctrl" in ops1020
assert "state_type_ne" in ops1020
assert "move_contact" in ops1020
assert ops1020.count("state_no_range") == 2
ops1000 = [i.op for i in rules[1].code]
assert "power_ge" not in ops1000
assert "ctrl" in ops1000
assert "move_contact" in ops1000
ops1120 = [i.op for i in rules[3].code]
assert "power_ge" in ops1120
assert "ctrl" in ops1120
assert "move_contact" in ops1120
ops1100 = [i.op for i in rules[4].code]
assert "power_ge" not in ops1100
ops1110 = [i.op for i in rules[5].code]
assert "power_ge" not in ops1110
ops1220 = [i.op for i in rules[6].code]
assert "power_ge" in ops1220
ops1200 = [i.op for i in rules[7].code]
assert "power_ge" not in ops1200
ops1210 = [i.op for i in rules[8].code]
assert "power_ge" not in ops1210
ops800 = [i.op for i in rules[10].code]
assert ops800.count("p2_body_dist_x_lt") == 2
assert ops800.count("p2_state_type_eq") == 4
assert ops800.count("p2_move_type_ne") == 2
assert "state_no_ne" in ops800
ops610 = [i.op for i in rules[11].code]
assert ops610.count("state_no_eq") == 2
assert "move_contact" in ops610
assert ops610.count("or") >= 2
print("ikemen state-rule compiler: OK")
