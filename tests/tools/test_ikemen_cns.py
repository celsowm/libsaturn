#!/usr/bin/env python3
from __future__ import annotations

import tempfile
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))

from tools.ikemen_cns import emit  # noqa: E402

SOURCE = r"""
[Data]
life = 1000
liedown.time = 60

[Size]
ground.back = 15
ground.front = 16
air.back = 12
air.front = 12
height = 60
attack.dist = 160

[Velocity]
walk.fwd = 2.4
walk.back = -2.2
run.fwd = 4.6,0
run.back = -4.5,-3.8
jump.neu = 0,-8.4
jump.back = -2.55
jump.fwd = 2.5
runjump.fwd = 4,-8.1
airjump.neu = 0,-8.1
airjump.back = -2.55
airjump.fwd = 2.5
air.gethit.groundrecover = -.15,-3.5
air.gethit.airrecover.mul = .5,.2
air.gethit.airrecover.add = 0,-4.5
air.gethit.airrecover.back = -1
air.gethit.airrecover.fwd = 0
air.gethit.airrecover.up = -2
air.gethit.airrecover.down = 1.5

[Movement]
airjump.num = 1
airjump.height = 35
yaccel = .44
stand.friction = .85
crouch.friction = .82
stand.friction.threshold = 2
crouch.friction.threshold = .05
air.gethit.groundlevel = 25
air.gethit.trip.groundlevel = 15
down.bounce.offset = 0,20
down.bounce.yaccel = .4
down.bounce.groundlevel = 12
down.friction.threshold = .05
air.gethit.groundrecover.ground.threshold = -20
air.gethit.groundrecover.groundlevel = 10
air.gethit.airrecover.threshold = -1
air.gethit.airrecover.yaccel = .35

[Statedef 200]
type = S
movetype = A
physics = S
velset = 0,0
ctrl = 0
anim = 200
poweradd = 10
sprpriority = 2

[State 200, Hit]
type = HitDef
trigger1 = AnimElem = 3
damage = 23, 0
animtype = Medium
guardflag = MA
guard.velocity = -3
guard.slidetime = 7
guard.hittime = 9
guard.ctrltime = 8
priority = 3, Hit
pausetime = 8, 8
sparkno = 0
sparkxy = -10, -76
hitsound = 5, 0
guardsound = 6, 0
ground.type = High
ground.slidetime = 5
ground.hittime = 11
ground.velocity = -4
air.velocity = -1.4,-3
air.hittime = 15

[State 200, Snd]
type = PlaySnd
trigger1 = Time = 1
value = 0, 0

[State 200, Priority]
type = SprPriority
trigger1 = AnimElem = 5
value = 2

[State 200, Pos]
type = PosAdd
trigger1 = AnimElem = 6
x = 12

[State 200, Ctrl]
type = CtrlSet
trigger1 = Time = 6
value = 1

[State 200, Width]
type = Width
trigger1 = (AnimElemTime (2) >= 0) && (AnimElemTime (7) < 0)
value = 15,0

[State 200, Skip Contact]
type = ChangeAnim
trigger1 = AnimElemTime(5) > 0 && AnimElemTime(6) <= 0
trigger1 = movecontact
ignorehitpause = 1
persistent = 0
value = 200
elem = 6

[State 200, End]
type = ChangeState
trigger1 = AnimTime = 0
value = 0
ctrl = 1
"""

COMMON = r"""
[StateDef 0; type: S; physics: S; sprpriority: 0;]
[StateDef 10; type: C; physics: C; anim: 10;]
[StateDef 11; type: C; physics: C; anim: 11; sprpriority: 0;]
[StateDef 12; type: S; physics: S; anim: 12;]
[StateDef 20; type: S; physics: S; sprpriority: 0;]
[StateDef 40; type: S; physics: S; anim: 40; ctrl: 0; sprpriority: 1; facep2: 1;]
[StateDef 45; type: A; physics: N; ctrl: 0; velset: 0, 0;]
[StateDef 50; type: A; physics: A;]
[StateDef 51; type: A; physics: A;]
[StateDef 52; type: S; physics: S; ctrl: 0; anim: 47;]
[StateDef 100; type: S; physics: S; anim: 100; sprpriority: 1;]
[StateDef 105; type: A; physics: A; ctrl: 0; anim: 105; sprpriority: 1;]
[StateDef 106; type: S; physics: S; ctrl: 0; anim: 47;]
[StateDef 120; type: U; physics: U;]
[StateDef 130; type: S; physics: S;]
[StateDef 131; type: C; physics: C;]
[StateDef 132; type: A; physics: N;]
[StateDef 140; type: U; physics: U; ctrl: 1;]
[StateDef 150; type: S; movetype: H; physics: N; velset: 0, 0;]
[StateDef 151; type: S; movetype: H; physics: S; anim: 150;]
[StateDef 152; type: C; movetype: H; physics: N; velset: 0, 0;]
[StateDef 153; type: C; movetype: H; physics: C; anim: 151;]
[StateDef 154; type: A; movetype: H; physics: N; velset: 0, 0;]
[StateDef 155; type: A; movetype: H; physics: N; anim: 152;]
[StateDef 5000; type: S; movetype: H; physics: N; velset: 0, 0;]
[StateDef 5001; type: S; movetype: H; physics: S;]
[StateDef 5010; type: C; movetype: H; physics: N; velset: 0, 0;]
[StateDef 5011; type: C; movetype: H; physics: C;]
[StateDef 5020; type: A; movetype: H; physics: N; velset: 0, 0;]
[StateDef 5030; type: A; movetype: H; physics: N; ctrl: 0;]
[StateDef 5040; type: A; movetype: H; physics: N;]
[StateDef 5050; type: A; movetype: H; physics: N;]
[StateDef 5035; type: A; movetype: H; physics: N;]
[StateDef 5070; type: A; movetype: H; physics: N; velset: 0, 0;]
[StateDef 5071; type: A; movetype: H; physics: N;]
[StateDef 5080; type: L; movetype: H; physics: N; velset: 0, 0;]
[StateDef 5081; type: L; movetype: H; physics: C;]
[StateDef 5100; type: L; movetype: H; physics: N;]
[StateDef 5101; type: L; movetype: H; physics: N;]
[StateDef 5110; type: L; movetype: H; physics: N;]
[StateDef 5120; type: L; movetype: I; physics: N;]
[StateDef 5150; type: L; movetype: H; physics: N; sprpriority: -3; ctrl: 0;]
[StateDef 5200; type: A; movetype: H; physics: N;]
[StateDef 5201; type: A; movetype: H; physics: A; anim: 5200;]
[StateDef 5210; type: A; movetype: I; physics: N; anim: 5210; ctrl: 0;]
"""

with tempfile.TemporaryDirectory() as td:
    root = Path(td)
    source = root / "kfm.cns"
    common = root / "common1.cns.zss"
    source.write_text(SOURCE, encoding="utf-8")
    common.write_text(COMMON, encoding="utf-8")
    report = emit(
        source, [200], root / "kfm_cns", "kfm",
        common, [0,10,11,12,20,40,45,50,51,52,100,105,106,
                 120,130,131,132,140,150,151,152,153,154,155,
                 5000,5001,5010,5011,5020,5030,5035,5040,5050,
                 5070,5071,5080,5081,5100,5101,5110,5120,5150,
                 5200,5201,5210]
    )

assert report["constants"]["walk_fwd_q8"] == round(2.4 * 256)
assert report["constants"]["yaccel_q8"] == round(.44 * 256)
assert report["constants"]["run_jump_fwd_x_q8"] == 4 * 256
assert report["constants"]["run_jump_fwd_y_q8"] == round(-8.1 * 256)
assert report["constants"]["air_jump_neu_y_q8"] == round(-8.1 * 256)
assert report["constants"]["air_jump_num"] == 1
assert report["constants"]["air_jump_height"] == 35
assert report["constants"]["attack_dist"] == 160
assert len(report["states"]) == 46
assert report["states"][0]["hitdef_count"] == 1
assert report["states"][0]["playsnd_count"] == 1
assert report["states"][0]["controller_count"] == 6
assert report["states"][0]["unsupported_controllers"] == []

hit = report["hitdefs"][0]
assert hit["trigger_kind"] == "IK_CNS_TRIGGER_ANIM_ELEM_EQ"
assert hit["trigger_value"] == 3
assert hit["damage"] == 23
assert hit["pause_p1"] == 8
assert hit["ground_velocity_x_q8"] == -4 * 256
assert hit["guard_flags"] == "IK_CNS_GUARD_STAND | IK_CNS_GUARD_CROUCH | IK_CNS_GUARD_AIR"
assert hit["guard_velocity_x_q8"] == -3 * 256
assert hit["guard_slide_time"] == 7
assert hit["guard_hit_time"] == 9
assert hit["guard_ctrl_time"] == 8
assert hit["anim_type"] == 1
assert hit["air_anim_type"] == 1
assert hit["fall_y_velocity_q8"] == round(-4.5 * 256)
assert hit["fall_x_velocity_set"] == 0
assert hit["fall_recover"] == 1
assert hit["fall_recover_time"] == 4

controllers = report["controllers"]
assert controllers[0]["type"] == "IK_CNS_CTRL_SPR_PRIORITY"
assert controllers[1]["type"] == "IK_CNS_CTRL_POS_ADD"
assert controllers[1]["value0"] == 12 * 256
assert controllers[2]["type"] == "IK_CNS_CTRL_CTRL_SET"
assert controllers[3]["type"] == "IK_CNS_CTRL_WIDTH"
assert controllers[3]["trigger_kind"] == "IK_CNS_TRIGGER_ANIM_ELEM_RANGE"
assert controllers[3]["trigger_value"] == 2
assert controllers[3]["trigger_value2"] == 7
assert controllers[3]["value0"] == 15
assert controllers[3]["value1"] == 0

assert controllers[4]["type"] == "IK_CNS_CTRL_CHANGE_ANIM"
assert controllers[4]["trigger_kind"] == "IK_CNS_TRIGGER_MOVE_CONTACT_ELEM_WINDOW"
assert controllers[4]["trigger_value"] == 5
assert controllers[4]["trigger_value2"] == 6
assert controllers[4]["value0"] == 200
assert controllers[4]["value1"] == 6
assert controllers[4]["flags"] == "IK_CNS_CTRL_IGNORE_HIT_PAUSE"

assert controllers[5]["type"] == "IK_CNS_CTRL_CHANGE_STATE"
assert controllers[5]["trigger_kind"] == "IK_CNS_TRIGGER_ANIM_END"
assert controllers[5]["flags"] == "IK_CNS_CTRL_HAS_CTRL"

print("ikemen CNS compiler: OK")

common_rows = {row["number"]: row for row in report["states"][1:]}
assert common_rows[0]["controller_count"] == 3
assert common_rows[20]["controller_count"] == 3
assert common_rows[40]["controller_count"] == 3
assert common_rows[45]["controller_count"] == 4
assert common_rows[45]["has_velset"] == 1
assert common_rows[50]["land_state"] == 52
assert common_rows[51]["land_state"] == 52
assert common_rows[52]["anim"] == 47
assert common_rows[105]["land_state"] == 106
assert common_rows[5080]["controller_count"] == 1
assert common_rows[5081]["controller_count"] == 3
assert common_rows[5110]["controller_count"] == 7
assert common_rows[5150]["spr_priority"] == -3
assert common_rows[5150]["controller_count"] == 3
assert common_rows[120]["controller_count"] == 2
assert common_rows[132]["land_state"] == 130
assert common_rows[140]["ctrl"] == 1
assert common_rows[150]["move_type"] == "IK_CNS_MOVE_HIT"
assert common_rows[151]["controller_count"] == 4
assert common_rows[155]["controller_count"] == 2
assert common_rows[5000]["controller_count"] == 3
assert common_rows[5001]["controller_count"] == 3
state_5001_ctrls = [
    c for c in report["controllers"] if c["state_number"] == 5001
]
assert state_5001_ctrls[1]["trigger_kind"] == "IK_CNS_TRIGGER_HIT_SLIDE_GE"
assert common_rows[5020]["controller_count"] == 2
assert common_rows[5030]["controller_count"] == 3
assert common_rows[5030]["land_level_q8"] == 25 * 256
assert common_rows[5035]["controller_count"] == 2
assert common_rows[5040]["land_state"] == 52
assert common_rows[5050]["land_state"] == 5100
assert common_rows[5050]["land_level_q8"] == 25 * 256
assert common_rows[5050]["controller_count"] == 1
assert common_rows[5071]["land_state"] == 5110
assert common_rows[5071]["land_level_q8"] == 15 * 256
assert common_rows[5100]["controller_count"] == 5
assert common_rows[5101]["anim"] == 5160
assert common_rows[5101]["air_accel_q8"] == round(.4 * 256)
assert common_rows[5101]["land_level_q8"] == 12 * 256
assert common_rows[5101]["land_state"] == 5110
assert common_rows[5120]["controller_count"] == 2
assert common_rows[5200]["land_state"] == 5201
assert common_rows[5200]["land_level_q8"] == 10 * 256
assert common_rows[5201]["land_state"] == 52
assert common_rows[5201]["controller_count"] == 2
assert common_rows[5210]["land_state"] == 52
assert common_rows[5210]["air_motion_start"] == 4
assert common_rows[5210]["land_ctrl"] == 1
assert common_rows[5210]["air_accel_q8"] == round(.35 * 256)
assert common_rows[5210]["controller_count"] == 7
assert report["common_deferred"][100] == ["AssertSpecial noWalk/noAutoTurn"]
assert report["common_deferred"][150] == ["ForceFeedback"]
assert report["constants"]["liedown_time"] == 60
assert report["constants"]["air_gethit_groundlevel_q8"] == 25 * 256
assert report["constants"]["air_gethit_trip_groundlevel_q8"] == 15 * 256
assert report["constants"]["down_bounce_offset_y_q8"] == 20 * 256
assert report["constants"]["down_bounce_yaccel_q8"] == round(.4 * 256)
assert report["constants"]["down_bounce_groundlevel_q8"] == 12 * 256
assert report["constants"]["air_gethit_groundrecover_x_q8"] == round(-.15 * 256)
assert report["constants"]["air_gethit_groundrecover_y_q8"] == round(-3.5 * 256)
assert report["constants"]["air_gethit_groundrecover_threshold_q8"] == -20 * 256
assert report["constants"]["air_gethit_groundrecover_groundlevel_q8"] == 10 * 256
assert report["constants"]["air_gethit_airrecover_mul_x_q8"] == round(.5 * 256)
assert report["constants"]["air_gethit_airrecover_add_y_q8"] == round(-4.5 * 256)
assert report["constants"]["air_gethit_airrecover_threshold_q8"] == -256
assert report["constants"]["air_gethit_airrecover_yaccel_q8"] == round(.35 * 256)
