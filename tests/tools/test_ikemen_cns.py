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

[Size]
ground.back = 15
ground.front = 16
air.back = 12
air.front = 12
height = 60

[Velocity]
walk.fwd = 2.4
walk.back = -2.2
run.fwd = 4.6,0
run.back = -4.5,-3.8
jump.neu = 0,-8.4
jump.back = -2.55
jump.fwd = 2.5

[Movement]
yaccel = .44
stand.friction = .85
crouch.friction = .82
stand.friction.threshold = 2
crouch.friction.threshold = .05

[Statedef 200]
type = S
movetype = A
physics = S
juggle = 1
velset = 0,0
ctrl = 0
anim = 200
poweradd = 10
sprpriority = 2

[State 200, Hit]
type = HitDef
trigger1 = AnimElem = 3
damage = 23, 0
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

[State 200, End]
type = ChangeState
trigger1 = AnimTime = 0
value = 0
ctrl = 1
"""

with tempfile.TemporaryDirectory() as td:
    root = Path(td)
    source = root / "kfm.cns"
    source.write_text(SOURCE, encoding="utf-8")
    report = emit(source, [200], root / "kfm_cns", "kfm")

assert report["constants"]["walk_fwd_q8"] == round(2.4 * 256)
assert report["constants"]["yaccel_q8"] == round(.44 * 256)
assert report["states"][0]["number"] == 200
assert report["states"][0]["anim"] == 200
assert report["states"][0]["hitdef_count"] == 1
assert report["states"][0]["playsnd_count"] == 1
assert report["states"][0]["unsupported_controllers"] == ["changestate"]

hit = report["hitdefs"][0]
assert hit["trigger_kind"] == "IK_CNS_TRIGGER_ANIM_ELEM_EQ"
assert hit["trigger_value"] == 3
assert hit["damage"] == 23
assert hit["pause_p1"] == 8
assert hit["ground_velocity_x_q8"] == -4 * 256
assert hit["air_velocity_y_q8"] == -3 * 256
assert hit["hit_sound_group"] == 5 and hit["hit_sound_item"] == 0

snd = report["playsnds"][0]
assert snd["trigger_kind"] == "IK_CNS_TRIGGER_TIME_EQ"
assert snd["trigger_value"] == 1
assert (snd["group"], snd["item"]) == (0, 0)

print("ikemen CNS compiler: OK")
