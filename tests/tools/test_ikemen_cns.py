#!/usr/bin/env python3
from __future__ import annotations

import tempfile
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))

from tools.ikemen_cns import (
    Section,
    compile_explod_controller,
    compile_helper_controller,
    compile_projectile_controller,
    controller_trigger,
    emit,
)  # noqa: E402

SOURCE = r"""
[Data]
life = 1000
liedown.time = 60
airjuggle = 15
sparkno = 2
guard.sparkno = 40

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
juggle = 5
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
down.velocity = -5,0
down.hittime = 22
down.bounce = 1
air.juggle = 2

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

[State 200, Generic VarSet]
type = VarSet
trigger1 = Time = 0
v = 7
value = 123456

[State 200, Generic VarAdd]
type = VarAdd
trigger1 = Time = 1
var(7) = 44

[State 200, Spawn Helper]
type = Helper
trigger1 = Time = 2
helpertype = normal
id = 77
stateno = 1234
pos = 12, -8
postype = p1
facing = -1
keyctrl = 1
ownpal = 1

[Statedef 1234]
type = S
movetype = I
physics = N
anim = 0

[State 1234, Destroy]
type = DestroySelf
trigger1 = Time = 5

[Statedef 800]
type = S
movetype = A
physics = S
juggle = 0
velset = 0,0
ctrl = 0
anim = 800
sprpriority = 2

[State 800, Throw]
type = HitDef
trigger1 = Time = 0
attr = S, NT
hitflag = M-
priority = 1, Miss
sparkno = -1
p1sprpriority = 1
p1facing = 1
p2facing = 1
p1stateno = 810
p2stateno = 820
guard.dist = 0
fall = 1

[State 800, End]
type = ChangeState
trigger1 = AnimTime = 0
value = 0
ctrl = 1

[Statedef 810]
type = S
movetype = A
physics = N
anim = 810

[State 810, Hold]
type = VarSet
trigger1 = Time = 0
var(2) = command = "holdfwd"

[State 810, Grab]
type = PlaySnd
trigger1 = AnimElem = 2
value = 1,1

[State 810, Bind 1]
type = TargetBind
trigger1 = AnimElemTime(2) < 0
pos = 28,0

[State 810, Width]
type = Width
trigger1 = AnimElemTime(2) >= 0 && AnimElemTime(12) < 0
edge = 60,0

[State 810, Bind 2]
type = TargetBind
trigger1 = AnimElemTime(2) >= 0 && AnimElemTime(5) < 0
pos = 58,0

[State 810, Turn]
type = Turn
trigger1 = var(2)
trigger1 = AnimElem = 6

[State 810, Pos]
type = PosAdd
trigger1 = var(2)
trigger1 = AnimElem = 6
x = -37

[State 810, Face]
type = TargetFacing
trigger1 = var(2)
trigger1 = AnimElem = 6
value = -1

[State 810, Bind 6]
type = TargetBind
trigger1 = AnimElemTime(6) >= 0 && AnimElemTime(7) < 0
pos = 41,-60

[State 810, Bind 11]
type = TargetBind
trigger1 = AnimElem = 11
pos = -50,-50

[State 810, Hurt]
type = TargetLifeAdd
trigger1 = AnimElem = 11
value = -78

[State 810, Throw]
type = TargetState
trigger1 = AnimElem = 11
value = 821

[State 810, Turn End]
type = Turn
trigger1 = AnimElem = 12

[State 810, End]
type = ChangeState
trigger1 = AnimTime = 0
value = 0
ctrl = 1

[Statedef 820]
type = A
movetype = H
physics = N
velset = 0,0

[State 820, Anim]
type = ChangeAnim2
trigger1 = Time = 0
value = 820

[State 820, Escape]
type = SelfState
trigger1 = !gethitvar(isbound)
value = 5050

[Statedef 821]
type = A
movetype = H
physics = N
velset = 2.8,-7
poweradd = 40

[State 821, Gravity]
type = VelAdd
trigger1 = 1
y = .4

[State 821, Ground Recover]
type = ChangeState
triggerall = Vel Y > 0
triggerall = Pos Y >= -20
triggerall = alive
triggerall = CanRecover
trigger1 = Command = "recovery"
value = 5200

[State 821, Air Recover]
type = SelfState
triggerall = Vel Y > 0
triggerall = alive
triggerall = CanRecover
trigger1 = Command = "recovery"
value = 5210

[State 821, Ground]
type = SelfState
trigger1 = Vel Y > 0
trigger1 = Pos Y >= 0
value = 5100

[Statedef 1000]
type = S
movetype = A
physics = S
juggle = 4
poweradd = 55
velset = 0,0
anim = 1000
ctrl = 0
sprpriority = 2

[State 1000, Snd]
type = PlaySnd
trigger1 = Time = 8
value = 0,3

[State 1000, Pos2]
type = PosAdd
trigger1 = AnimElem = 2
x = 20

[State 1000, Pos3]
type = PosAdd
trigger1 = AnimElem = 3
trigger2 = AnimElem = 13
x = 10

[State 1000, Pos5]
type = PosAdd
trigger1 = AnimElem = 5
x = 5

[State 1000, Near]
type = HitDef
trigger1 = AnimElem = 5
trigger1 = p2bodydist X < 40
attr = S, SA
animtype = Hard
damage = 90,4
priority = 5
guardflag = MA
pausetime = 15,15
ground.type = Low
ground.slidetime = 12
ground.hittime = 17
ground.velocity = -4,-3.5
air.velocity = -4,-3
fall = 1

[State 1000, Far]
type = HitDef
trigger1 = AnimElem = 5
trigger1 = p2bodydist X >= 40
attr = S, SA
animtype = Hard
damage = 85,4
priority = 4
guardflag = MA
pausetime = 15,15
ground.type = Low
ground.slidetime = 12
ground.hittime = 17
ground.velocity = -7
air.velocity = -4,-2.5

[State 1000, Pos9]
type = PosAdd
trigger1 = AnimElem = 9
x = -5

[State 1000, End]
type = ChangeState
trigger1 = AnimTime = 0
value = 0
ctrl = 1

[Statedef 1010]
type = S
movetype = A
physics = S
juggle = 4
poweradd = 60
velset = 0,0
anim = 1010
ctrl = 0
sprpriority = 2

[State 1010, Snd]
type = PlaySnd
trigger1 = Time = 9
value = 0,3

[State 1010, Pos2]
type = PosAdd
trigger1 = AnimElem = 2
x = 20

[State 1010, Pos3]
type = PosAdd
trigger1 = AnimElem = 3
trigger2 = AnimElem = 13
x = 10

[State 1010, Pos5]
type = PosAdd
trigger1 = AnimElem = 5
x = 5

[State 1010, Vel5]
type = VelSet
trigger1 = AnimElem = 5
x = 4

[State 1010, Near]
type = HitDef
trigger1 = AnimElem = 5
trigger1 = p2bodydist X < 40
attr = S, SA
animtype = Hard
damage = 90,4
priority = 5
guardflag = MA
pausetime = 15,15
ground.type = Low
ground.slidetime = 12
ground.hittime = 17
ground.velocity = -4,-3.5
air.velocity = -4,-3
fall = 1

[State 1010, Far]
type = HitDef
trigger1 = AnimElem = 5
trigger1 = p2bodydist X >= 40
attr = S, SA
animtype = Hard
damage = 85,4
priority = 4
guardflag = MA
pausetime = 15,15
ground.type = Low
ground.slidetime = 12
ground.hittime = 17
ground.velocity = -7
air.velocity = -4,-2.5

[State 1010, End]
type = ChangeState
trigger1 = AnimTime = 0
value = 0
ctrl = 1

[Statedef 1020]
type = S
movetype = A
physics = N
juggle = 6
poweradd = -330
velset = 0,0
anim = 1020
ctrl = 0
sprpriority = 2

[State 1020, Friction]
type = VelMul
trigger1 = 1
x = .85 * ifelse (AnimElemTime(6) < 0, 1, .8)

[State 1020, Afterimage]
type = AfterImage
trigger1 = Time = 0
length = 13

[State 1020, AfterimageTime]
type = AfterImageTime
trigger1 = AnimElemTime(8) < 0
time = 2

[State 1020, PalFX]
type = PalFX
trigger1 = Time = 0
time = 20

[State 1020, Snd]
type = PlaySnd
trigger1 = Time = 2
value = 0,3

[State 1020, Pos2]
type = PosAdd
trigger1 = AnimElem = 2
x = 20

[State 1020, Pos3]
type = PosAdd
trigger1 = AnimElem = 3
trigger2 = AnimElem = 12
x = 10

[State 1020, Pos4]
type = PosAdd
trigger1 = AnimElem = 4
x = 5

[State 1020, Vel4]
type = VelSet
trigger1 = AnimElem = 4
x = 13

[State 1020, Hit]
type = HitDef
trigger1 = AnimElem = 4
attr = S, SA
animtype = Hard
damage = 95,5
priority = 4
guardflag = MA
pausetime = 8,7
ground.type = Low
ground.slidetime = 20
ground.hittime = 22
ground.velocity = -8,-7
guard.velocity = -7
air.velocity = -8,-7
airguard.velocity = -5,-4
fall = 1
p2stateno = 1025
p2facing = 1

[State 1020, End]
type = ChangeState
trigger1 = AnimTime = 0
value = 0
ctrl = 1

[Statedef 1025]
type = A
movetype = H
physics = N
velset = 0,0

[State 1025, Anim]
type = ChangeAnim2
trigger1 = 1
value = 1025

[State 1025, State]
type = ChangeState
trigger1 = HitShakeOver = 1
value = 1026

[Statedef 1026]
type = A
movetype = H
physics = N

[State 1026, Velocity]
type = HitVelSet
trigger1 = Time = 0
x = 1
y = 1

[State 1026, Gravity]
type = VelAdd
trigger1 = 1
y = .45

[State 1026, No scroll]
type = ScreenBound
triggerall = Pos y < -15
trigger1 = BackEdgeBodyDist < 65
trigger2 = FrontEdgeBodyDist < 65
value = 1
movecamera = 0,1

[State 1026, Hit wall]
type = ChangeState
triggerall = Pos y < -15
trigger1 = BackEdgeBodyDist <= 20
trigger2 = FrontEdgeBodyDist <= 20
value = 1027

[State 1026, Hit ground]
type = SelfState
trigger1 = (Vel y > 0) && (Pos y >= 0)
value = 5100

[Statedef 1027]
type = A
movetype = H
physics = N

[State 1027, Turn]
type = Turn
trigger1 = (Time = 0) && (FrontEdgeBodyDist <= 30)

[State 1027, Pos]
type = PosAdd
trigger1 = Time = 0
x = 15 - BackEdgeBodyDist

[State 1027, Stop moving]
type = PosFreeze
trigger1 = 1
x = 1
y = 1

[State 1027, No scroll]
type = ScreenBound
trigger1 = 1
value = 1
movecamera = 0,1

[State 1027, Spark]
type = Explod
trigger1 = Time = 0
anim = F72
pos = 0,0

[State 1027, Anim]
type = ChangeAnim2
trigger1 = Time = 0
value = 1027

[State 1027, Sound]
type = PlaySnd
trigger1 = Time = 0
value = F7,0

[State 1027, State]
type = ChangeState
trigger1 = AnimTime = 0
value = 1028

[Statedef 1028]
type = A
movetype = H
physics = N

[State 1028, No normal]
type = NotHitBy
trigger1 = 1
value = , NA, NP

[State 1028, Vel Y]
type = VelSet
trigger1 = Time = 0
y = -6

[State 1028, Vel X]
type = VelSet
trigger1 = Time = 0
x = 1.6

[State 1028, Turn]
type = Turn
trigger1 = (Time = 0) && (BackEdgeDist < 30)

[State 1028, Gravity]
type = VelAdd
trigger1 = 1
y = .35

[State 1028, Anim 5050]
type = ChangeAnim
trigger1 = Time = 0
trigger1 = !SelfAnimExist(5052)
value = 5050

[State 1028, Anim 5052]
type = ChangeAnim
trigger1 = Time = 0
trigger1 = SelfAnimExist(5052)
value = 5052

[State 1028, Anim 5060]
type = ChangeAnim
trigger1 = Vel Y > -2
trigger1 = Anim = 5050
trigger1 = SelfAnimExist(5060)
persistent = 0
value = 5060

[State 1028, Anim 5062]
type = ChangeAnim
trigger1 = Vel Y > -2
trigger1 = Anim = 5052
trigger1 = SelfAnimExist(5062)
persistent = 0
value = 5062

[State 1028, Hit ground]
type = SelfState
trigger1 = (Vel y > 0) && (Pos y >= 0)
value = 5100

[Statedef 1050]
type = S
movetype = A
physics = S
juggle = 4
poweradd = 55
velset = 0,0
anim = 1050
ctrl = 0
sprpriority = 2

[State 1050, Snd]
type = PlaySnd
trigger1 = Time = 1
value = 0,2

[State 1050, Disabled Pos]
type = null;PosAdd
trigger1 = AnimElem = 2
x = 15

[State 1050, Pos]
type = PosAdd
trigger1 = AnimElem = 4
x = 20

[State 1050, Hit]
type = HitDef
trigger1 = Time = 0
attr = A, SA
animtype = Medium
damage = 80,4
priority = 5
guardflag = MA
pausetime = 12,12
ground.type = Low
ground.slidetime = 20
ground.hittime = 22
ground.velocity = -3.5,-7
air.velocity = -3.5,-7
fall = 1

[State 1050, Jump]
type = ChangeState
trigger1 = AnimTime = 0
value = 1051

[Statedef 1051]
type = A
movetype = A
physics = N
velset = 2,-6
anim = 1051
hitdefpersist = 1

[State 1051, Gravity]
type = VelAdd
trigger1 = 1
y = .45

[State 1051, Kick]
type = ChangeState
trigger1 = Command = "a" || Command = "b"
trigger1 = Vel y < -1
value = 1055

[State 1051, Land]
type = ChangeState
trigger1 = Vel Y > 0 && Pos Y >= -10
value = 1052

[Statedef 1052]
type = S
movetype = I
physics = S
anim = 1052
sprpriority = 1
velset = 0,0

[State 1052, Ground]
type = PosSet
trigger1 = Time = 0
y = 0

[State 1052, Land sound]
type = PlaySnd
trigger1 = Time = 0
value = 40,0

[State 1052, Early ctrl]
type = CtrlSet
trigger1 = AnimElem = 3, -1
value = 1

[State 1052, Back]
type = PosAdd
trigger1 = AnimElem = 4
x = -15

[State 1052, End]
type = ChangeState
trigger1 = AnimTime = 0
value = 0
ctrl = 1

[Statedef 1055]
type = A
movetype = A
physics = N
anim = 1055

[State 1055, Snd1]
type = PlaySnd
trigger1 = Time = 0
value = 100,0

[State 1055, Snd2]
type = PlaySnd
trigger1 = Time = 0
value = 0,1

[State 1055, Pos]
type = PosAdd
trigger1 = Time = 0
x = 10
y = -10

[State 1055, Gravity]
type = VelAdd
trigger1 = 1
y = .45

[State 1055, Hit]
type = HitDef
trigger1 = Time = 0
attr = A, SA
animtype = Med
damage = 35 + (prevstateno = 1061)*5, 2
priority = 4
guardflag = MA
pausetime = 12,12
ground.type = High
ground.slidetime = 15
ground.hittime = 18
ground.velocity = -6
air.velocity = -4,-5
air.fall = 1

[State 1055, Land]
type = ChangeState
trigger1 = Vel Y > 0 && Pos Y >= -5
value = 1056

[Statedef 1056]
type = S
movetype = I
physics = S
anim = 1056
sprpriority = 1
velset = 0,0

[State 1056, Ground]
type = PosSet
trigger1 = Time = 0
y = 0

[State 1056, Snd]
type = PlaySnd
trigger1 = Time = 0
value = 40,0

[State 1056, End]
type = ChangeState
trigger1 = AnimTime = 0
value = 0
ctrl = 1

[Statedef 1060]
type = S
movetype = A
physics = S
juggle = 4
poweradd = 60
velset = 0,0
anim = 1060
ctrl = 0
sprpriority = 2

[State 1060, Pos2]
type = PosAdd
trigger1 = AnimElem = 2
x = 6

[State 1060, Pos4]
type = PosAdd
trigger1 = AnimElem = 4
x = 21

[State 1060, Hit]
type = HitDef
trigger1 = Time = 0
attr = A, SA
animtype = Medium
damage = 90,4
priority = 5
guardflag = MA
pausetime = 12,12
ground.type = Low
ground.slidetime = 20
ground.hittime = 22
ground.velocity = -3.5,-7.5
air.velocity = -3.5,-7.5
fall = 1

[State 1060, Jump]
type = ChangeState
trigger1 = AnimTime = 0
value = 1061

[Statedef 1061]
type = A
movetype = A
physics = N
velset = 2.5,-7.5
anim = 1061
hitdefpersist = 1

[State 1061, Gravity]
type = VelAdd
trigger1 = 1
y = .45

[State 1061, Kick]
type = ChangeState
trigger1 = Command = "a" || Command = "b"
trigger1 = Vel y < -1
value = 1055

[State 1061, Land]
type = ChangeState
trigger1 = Vel Y > 0 && Pos Y >= -10
value = 1052

[Statedef 1100]
type = S
movetype = A
physics = S
juggle = 4
poweradd = 55
velset = 0,0
anim = 1100
ctrl = 0
sprpriority = 2

[State 1100, Width]
type = Width
trigger1 = AnimElemTime(4) >= 0 && AnimElemTime(13) < 0
value = 5,0

[State 1100, Snd]
type = PlaySnd
trigger1 = AnimElem = 4
value = 0,2

[State 1100, First]
type = HitDef
trigger1 = Time = 0
attr = S, SA
animtype = Med
damage = 52,4
priority = 5
guardflag = MA
pausetime = 4,8
ground.type = Low
ground.slidetime = 15
ground.hittime = 20
ground.velocity = -3
air.velocity = -2,-2
p2facing = 1
forcestand = 1

[State 1100, Second]
type = HitDef
trigger1 = AnimElem = 7
attr = S, SA
animtype = Up
damage = 55,4
priority = 5
guardflag = MA
pausetime = 12,12
ground.type = Low
ground.slidetime = 15
ground.hittime = 20
ground.velocity = -1,-9.5
air.velocity = -1,-7.5
p2facing = 1
fall = 1
fall.recovertime = 40
yaccel = .4

[State 1100, End]
type = ChangeState
trigger1 = AnimTime = 0
value = 0
ctrl = 1

[Statedef 1110]
type = S
movetype = A
physics = S
juggle = 4
poweradd = 60
velset = 0,0
anim = 1110
ctrl = 0
sprpriority = 2

[State 1110, Width]
type = Width
trigger1 = AnimElemTime(4) >= 0 && AnimElemTime(14) < 0
value = 5,0

[State 1110, Snd]
type = PlaySnd
trigger1 = AnimElem = 4
value = 0,2

[State 1110, First]
type = HitDef
trigger1 = Time = 0
attr = S, SA
animtype = Med
damage = 57,4
priority = 5
guardflag = MA
pausetime = 4,8
ground.type = Low
ground.slidetime = 15
ground.hittime = 20
ground.velocity = -3
air.velocity = -2,-2
p2facing = 1
forcestand = 1

[State 1110, Second]
type = HitDef
trigger1 = AnimElem = 7
attr = S, SA
animtype = Up
damage = 60,4
priority = 5
guardflag = MA
pausetime = 12,12
ground.type = Low
ground.slidetime = 15
ground.hittime = 20
ground.velocity = -1,-10.5
air.velocity = -1,-8.5
p2facing = 1
fall = 1
fall.recovertime = 50
yaccel = .4

[State 1110, End]
type = ChangeState
trigger1 = AnimTime = 0
value = 0
ctrl = 1

[Statedef 1120]
type = S
movetype = A
physics = S
juggle = 6
poweradd = -330
velset = 0,0
anim = 1120
ctrl = 0
sprpriority = 2

[State 1120, Width]
type = Width
trigger1 = AnimElemTime(4) >= 0 && AnimElemTime(14) < 0
value = 5,0

[State 1120, Afterimage]
type = AfterImage
trigger1 = Time = 0
length = 13

[State 1120, AfterimageTime]
type = AfterImageTime
trigger1 = AnimTime < -2
time = 2

[State 1120, PalFX]
type = PalFX
trigger1 = Time = 0
time = 20

[State 1120, Snd]
type = PlaySnd
trigger1 = AnimElem = 4
value = 0,2

[State 1120, Rehit]
type = HitDef
trigger1 = Time = 0
trigger2 = AnimElem = 4
attr = S, SA
animtype = Med
damage = 30,4
priority = 5
guardflag = MA
pausetime = 6,10
sparkxy = 0, ifelse(Time = 0, -48, -55)
ground.type = Low
ground.slidetime = 18
ground.hittime = 23
ground.velocity = -3
air.velocity = -2,-2
p2facing = 1
forcestand = 1

[State 1120, Finish]
type = HitDef
trigger1 = AnimElem = 7
attr = S, SA
animtype = Up
damage = 68,4
priority = 5
guardflag = MA
pausetime = 12,12
ground.type = Low
ground.slidetime = 18
ground.hittime = 23
ground.velocity = -1.2,-11
air.velocity = -1.2,-9
p2facing = 1
fall = 1
fall.recovertime = 60
yaccel = .4

[State 1120, End]
type = ChangeState
trigger1 = AnimTime = 0
value = 0
ctrl = 1

[Statedef 1200]
type = S
movetype = A
physics = S
juggle = 4
poweradd = 50
velset = 0,0
anim = 1200
ctrl = 0
sprpriority = 2

[State 1200, Snd]
type = PlaySnd
trigger1 = AnimElem = 4
value = 0,3

[State 1200, Width 1]
type = Width
trigger1 = AnimElemTime(5) >= 0 && AnimElemTime(6) < 0
value = 10,0

[State 1200, Width 2]
type = Width
trigger1 = AnimElemTime(6) >= 0 && AnimElemTime(9) < 0
value = 20,0

[State 1200, Shake]
type = EnvShake
trigger1 = AnimElem = 6
time = 4

[State 1200, Hit]
type = HitDef
trigger1 = Time = 0
attr = S, SA
animtype = Hard
damage = 100,6
priority = 5
guardflag = MA
pausetime = 12,12
ground.type = Low
ground.slidetime = 16
ground.hittime = 20
ground.velocity = -10
ground.cornerpush.veloff = -12
guard.velocity = -7
air.velocity = -3.5,-4.5

[State 1200, End]
type = ChangeState
trigger1 = AnimTime = 0
value = 0
ctrl = 1

[Statedef 1210]
type = S
movetype = A
physics = S
juggle = 4
poweradd = 60
velset = 0,0
anim = 1210
ctrl = 0
sprpriority = 2

[State 1210, Snd]
type = PlaySnd
trigger1 = AnimElem = 4
value = 0,3

[State 1210, Width 1]
type = Width
trigger1 = AnimElemTime(5) >= 0 && AnimElemTime(6) < 0
value = 10,0

[State 1210, Width 2]
type = Width
trigger1 = AnimElemTime(6) >= 0 && AnimElemTime(9) < 0
value = 20,0

[State 1210, Shake]
type = EnvShake
trigger1 = AnimElem = 6
time = 8

[State 1210, Hit]
type = HitDef
trigger1 = Time = 0
attr = S, SA
animtype = Hard
damage = 125,9
priority = 5
guardflag = MA
pausetime = 12,12
ground.type = Low
ground.slidetime = 18
ground.hittime = 22
ground.velocity = -10
ground.cornerpush.veloff = -15
guard.velocity = -8
air.velocity = -4,-4.5

[State 1210, End]
type = ChangeState
trigger1 = AnimTime = 0
value = 0
ctrl = 1

[Statedef 1220]
type = S
movetype = A
physics = S
juggle = 6
poweradd = -330
velset = 0,0
anim = 1220
ctrl = 0
sprpriority = 2

[State 1220, Afterimage]
type = AfterImage
trigger1 = Time = 0
length = 13

[State 1220, AfterimageTime]
type = AfterImageTime
trigger1 = AnimTime < -2
time = 2

[State 1220, PalFX]
type = PalFX
trigger1 = Time = 0
time = 20

[State 1220, Snd]
type = PlaySnd
trigger1 = AnimElem = 4
value = 0,3

[State 1220, Width 1]
type = Width
trigger1 = AnimElemTime(5) >= 0 && AnimElemTime(6) < 0
value = 10,0

[State 1220, Width 2]
type = Width
trigger1 = AnimElemTime(6) >= 0 && AnimElemTime(9) < 0
value = 20,0

[State 1220, Shake]
type = EnvShake
trigger1 = AnimElem = 6
time = 8

[State 1220, Hit]
type = HitDef
trigger1 = Time = 0
attr = S, SA
animtype = Hard
damage = 125,9
priority = 5
guardflag = MA
pausetime = 15,15
ground.type = Low
ground.slidetime = 20
ground.hittime = 32
ground.velocity = -15
ground.cornerpush.veloff = -20
guard.velocity = -9
air.velocity = -5,-5
air.fall = 1
yaccel = .4

[State 1220, End]
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
        source, [200,1234,800,810,820,821,1000,1010,1020,1025,1026,1027,1028,1050,1051,1052,1055,1056,1060,1061,1100,1110,1120,1200,1210,1220], root / "kfm_cns", "kfm",
        common, [0,10,11,12,20,40,45,50,51,52,100,105,106,
                 120,130,131,132,140,150,151,152,153,154,155,
                 5000,5001,5010,5011,5020,5030,5035,5040,5050,
                 5070,5071,5080,5081,5100,5101,5110,5120,5150,
                 5200,5201,5210]
    )

    reversal_source = root / "blocking.cns"
    reversal_source.write_text(r"""
[Movement]
yaccel = .44

[Statedef 1300]
type = S
movetype = I
physics = S
anim = 1300

[State 1300, Width]
type = Width
trigger1 = AnimElemTime(3) < 0
value = 15,0

[State 1300, Start]
type = ReversalDef
trigger1 = Time = 0
reversal.attr = SA, AA
pausetime = 0,0
sparkno = 40
sparkxy = 40,0
hitsound = 6,0
p1stateno = 1310
p1sprpriority = 2
p2sprpriority = 1

[State 1300, Stop]
type = ReversalDef
trigger1 = Time = 4
trigger2 = Time = 8
reversal.attr =

[State 1300, Projectile fallback]
type = HitOverride
trigger1 = Time = 0
attr = SA, AP
stateno = 1310
time = 8

[Statedef 1310]
type = S
movetype = I
physics = S
anim = 1310

[State 1310, Pause]
type = Pause
trigger1 = Time = 0
time = 20
endcmdbuftime = 20
pausebg = 0

[State 1310, Invulnerable]
type = NotHitBy
trigger1 = Time = 0
value = SCA
time = 1

[Statedef 1340]
type = A
movetype = I
physics = N
anim = 1340

[State 1340, Start]
type = ReversalDef
trigger1 = Time = 0
reversal.attr = A, AA
p1stateno = 1350

[State 1340, Stop]
type = ReversalDef
trigger1 = Time = 5
reversal.attr =

[State 1340, Gravity]
type = VelAdd
trigger1 = 1
y = Const(movement.yaccel)

[Statedef 1350]
type = A
movetype = I
physics = N
anim = 1350

[State 1350, Freeze]
type = PosFreeze
trigger1 = AnimElemTime(3) < 0

[State 1350, Gravity]
type = VelAdd
trigger1 = AnimElemTime(3) > 0
y = Const(movement.yaccel)
""", encoding="utf-8")
    reversal_report = emit(
        reversal_source, [1300, 1310, 1340, 1350],
        root / "blocking_cns", "blocking"
    )

    projectile_source = root / "projectile.cns"
    projectile_source.write_text(r"""
[Statedef 9000]
type = S
movetype = A
physics = N
anim = 0
ctrl = 0

[State 9000, Shot]
type = Projectile
trigger1 = Time = 0
projid = 42
projanim = 9001
projhitanim = 9002
projremanim = 9003
projcancelanim = 9004
offset = 30,-20
velocity = 4,0
velmul = 1,1
projremove = 0
projremovetime = 90
projhits = 2
projmisstime = 5
projpriority = 2
projsprpriority = 4
projedgebound = 50
projstagebound = 60
attr = S, SP
damage = 40,5
priority = 4, Hit
pausetime = 6,8
guardflag = MA
ground.type = Low
ground.slidetime = 12
ground.hittime = 18
ground.velocity = -4,0
air.velocity = -3,-4

[State 9000, Direct Hit After Projectile]
type = HitDef
trigger1 = AnimElem = 1
attr = S, NA
damage = 99,0
priority = 4
guardflag = MA
ground.type = High
ground.slidetime = 8
ground.hittime = 10
ground.velocity = -2,0
air.velocity = -2,-2
""", encoding="utf-8")
    projectile_report = emit(
        projectile_source, [9000],
        root / "projectile_cns", "projectile"
    )

    intro_source = root / "intro.cns"
    intro_source.write_text(r"""
[Statedef 191]
type = S
ctrl = 0
anim = 190
velset = 0,0

[State 191, Freeze]
type = ChangeAnim
trigger1 = RoundState = 0
value = 190

[State 191, Intro]
type = AssertSpecial
trigger1 = 1
flag = Intro

[State 191, End]
type = ChangeState
trigger1 = AnimTime = 0
value = 0

[State 191, Wood 1]
type = Explod
trigger1 = RoundState != 0
persistent = 0
anim = 191
postype = p1
pos = 260,-90
velocity = -4.2,-7
accel = 0,.32
removetime = 48

[State 191, Wood 2]
type = Explod
trigger1 = AnimElemTime(7) = 1
anim = 192
postype = p1
pos = 60,-70
velocity = 2,-4
accel = 0,.32
removetime = 35

[State 191, Snd 1]
type = PlaySnd
trigger1 = AnimElem = 7
value = F5,2
volume = -40

[State 191, Snd 2]
type = PlaySnd
trigger1 = AnimElemTime(7) = 3
value = F5,3
volume = -80
""", encoding="utf-8")
    intro_report = emit(
        intro_source, [191],
        root / "intro_cns", "intro"
    )

    zankou_source = root / "zankou.cns"
    zankou_source.write_text(r"""
[Statedef 1400]
type = S
movetype = A
physics = N
juggle = 4
poweradd = 50
velset = 0,0
anim = 1400
ctrl = 0
sprpriority = 2

[State 1400, Friction]
type = VelMul
trigger1 = 1
x = 0.5

[State 1400, Hit]
type = HitDef
trigger1 = Time = 0
attr = S, SA
damage = 100,6
priority = 4
guardflag = MA
pausetime = 12,12
ground.type = Low
ground.slidetime = 12
ground.hittime = 17
ground.velocity = -9
guard.velocity = -9
air.velocity = -2,-5
air.fall = 1

[State 1400, Step]
type = PosAdd
trigger1 = AnimElem = 2
trigger2 = AnimElem = 3
trigger3 = AnimElem = 4
x = 10

[Statedef 1420]
type = S
movetype = A
physics = N
juggle = 6
poweradd = -330
velset = 0,0
anim = 1420
ctrl = 0
sprpriority = 2

[State 1420, Afterimage]
type = AfterImage
trigger1 = Time = 0
length = 13
PalBright = 30,30,0
PalContrast = 70,70,20
PalAdd = -10,-10,-10
PalMul = .85,.85,.50
TimeGap = 1
FrameGap = 2
time = 2

[State 1420, Blink]
type = PalFX
trigger1 = Time = 0
time = 20
add = 32,16,0
mul = 256,192,128
sinadd = 64,32,5,3
sinmul = 0,-64,-128,5

[State 1420, Early Hit]
type = HitDef
trigger1 = AnimElemTime(4) = -2
attr = S, SA
damage = 25,2
priority = 4
guardflag = MA
pausetime = 9,9
ground.type = Low
ground.slidetime = 22
ground.hittime = 24
ground.velocity = -7
air.velocity = -5,-4

[State 1420, Main Hit]
type = HitDef
trigger1 = AnimElem = 4
attr = S, SA
damage = 100,8
priority = 5
guardflag = MA
pausetime = 12,12
ground.type = Low
ground.slidetime = 22
ground.hittime = 24
ground.velocity = -5,-4
air.velocity = -5,-4
fall = 1

[State 1420, Dash]
type = VelSet
trigger1 = AnimElemTime(3) = [1,2]
x = 20
""", encoding="utf-8")
    zankou_report = emit(
        zankou_source, [1400, 1420],
        root / "zankou_cns", "zankou"
    )

    super_source = root / "supers.cns"
    super_source.write_text(r"""
[Statedef 3000]
type = S
movetype = A
physics = S
juggle = 4
velset = 0,0
anim = 3000
ctrl = 0
sprpriority = 2

[State 3000, Width]
type = Width
trigger1 = AnimElem = 2, >= 0
value = 15,0

[State 3000, SuperPause]
type = SuperPause
trigger1 = AnimElem = 2, 1
sound = 20,0
poweradd = -1000

[State 3000, AfterImage]
type = AfterImage
trigger1 = AnimElem = 2, 1
time = 2

[State 3000, Invuln]
type = NotHitBy
trigger1 = AnimElem = 2
value = , NA, SA, AT
time = 11

[State 3000, Crouch NA Invuln]
type = NotHitBy
trigger1 = AnimElemTime(2) >= 0 && AnimElemTime(14) < 0
value2 = C, NA
time = 1

[State 3000, Voice]
type = PlaySnd
trigger1 = AnimElem = 4
trigger2 = AnimElem = 12
trigger3 = AnimElem = 20
value = 0,3

[State 3000, Steps]
type = PosAdd
trigger1 = AnimElem = 3
trigger2 = AnimElem = 11
trigger3 = AnimElem = 13
trigger4 = AnimElem = 19
trigger5 = AnimElem = 21
trigger4 = AnimElem = 31
x = 10

[State 3000, Hit A]
type = HitDef
trigger1 = AnimElem = 5
trigger2 = AnimElem = 13
attr = S, HA
damage = 72,4
priority = 6
guardflag = MA
pausetime = 15,15
ground.type = Low
ground.slidetime = 30
ground.hittime = 32
ground.velocity = -6
air.velocity = -3,-2.8
air.fall = 1
fall.recover = 0

[State 3000, Hit B]
type = HitDef
trigger1 = AnimElem = 21
attr = S, HA
damage = 75,4
priority = 5
guardflag = MA
pausetime = 15,15
ground.type = Low
ground.slidetime = 30
ground.hittime = 32
ground.velocity = -5,-4
air.velocity = -5,-4
fall = 1
fall.recover = 0

[Statedef 3050]
type = S
movetype = A
physics = S
juggle = 4
velset = 0,0
anim = 3050
ctrl = 0
sprpriority = 2

[State 3050, Width]
type = Width
trigger1 = AnimElemTime(4) >= 0 && AnimElemTime(16) < 0
value = 5,0

[State 3050, SuperPause]
type = SuperPause
trigger1 = AnimElem = 2
poweradd = -1000

[State 3050, Invuln]
type = NotHitBy
trigger1 = AnimElem = 2
value = , NA, SA, AT
time = 6

[State 3050, Hit]
type = HitDef
trigger1 = Time = 0
attr = S, HA
damage = 155,12
priority = 5
guardflag = MA
pausetime = 30,30
ground.type = Low
ground.slidetime = 26
ground.hittime = 28
ground.velocity = -1.3,-25
air.velocity = -1.3,-25
fall = 1
fall.recover = 0
yaccel = .8
envshake.time = 25
envshake.ampl = 7
envshake.freq = 176
fall.damage = 70
fall.envshake.time = 15
fall.envshake.ampl = 6
fall.envshake.freq = 178

[State 3050, Success]
type = ChangeState
trigger1 = MoveHit
value = 3051

[State 3050, End]
type = ChangeState
trigger1 = AnimTime = 0
value = 0
ctrl = 1

[Statedef 3051]
type = S
movetype = A
physics = S
anim = 3051

[State 3051, AfterImage]
type = AfterImageTime
trigger1 = AnimTime < -2
time = 2

[State 3051, End]
type = ChangeState
trigger1 = AnimTime = 0
value = 0
ctrl = 1
""", encoding="utf-8")
    super_report = emit(
        super_source, [3000, 3050, 3051],
        root / "super_cns", "supers"
    )

projectile_row = projectile_report["states"][0]
assert projectile_row["hitdef_count"] == 1
assert len(projectile_report["hitdefs"]) == 2
assert len(projectile_report["projectiles"]) == 1
assert projectile_report["projectiles"][0]["hitdef_global"] == 1
assert projectile_report["hitdefs"][0]["damage"] == 99
assert projectile_report["hitdefs"][0]["attack_attr_mask"] == (
    "IK_CNS_ATTR_NORMAL_ATTACK"
)
assert projectile_report["hitdefs"][1]["attack_attr_mask"] == (
    "IK_CNS_ATTR_SPECIAL_PROJECTILE"
)
assert any(
    c["type"] == "IK_CNS_CTRL_PROJECTILE"
    for c in projectile_report["controllers"]
)

intro_row = intro_report["states"][0]
assert intro_row["number"] == 191
assert intro_row["unsupported_controllers"] == []
intro_ctrls = intro_report["controllers"]
assert intro_ctrls[0]["type"] == "IK_CNS_CTRL_CHANGE_ANIM"
assert intro_ctrls[0]["trigger_kind"] == "IK_CNS_TRIGGER_ROUND_STATE_EQ"
assert intro_ctrls[1]["type"] == "IK_CNS_CTRL_ASSERT_INTRO"
assert intro_ctrls[2]["type"] == "IK_CNS_CTRL_CHANGE_STATE"
intro_explod_ctrls = [
    c for c in intro_ctrls if c["type"] == "IK_CNS_CTRL_EXPLOD"
]
assert len(intro_explod_ctrls) == 2
assert intro_explod_ctrls[0]["trigger_kind"] == "IK_CNS_TRIGGER_ROUND_STATE_NE"
assert intro_explod_ctrls[0]["value1"] == 1
assert intro_explod_ctrls[1]["trigger_kind"] == (
    "IK_CNS_TRIGGER_ANIM_ELEM_TIME_EQ_PACKED"
)
assert len(intro_report["explods"]) == 2
assert intro_report["explods"][0]["anim_no"] == 191
assert intro_report["explods"][1]["anim_no"] == 192
assert len(intro_report["playsnds"]) == 2
assert intro_report["playsnds"][0]["group"] == 5
assert intro_report["playsnds"][0]["item"] == 2
assert intro_report["playsnds"][1]["group"] == 5
assert intro_report["playsnds"][1]["item"] == 3

assert report["constants"]["walk_fwd_q8"] == round(2.4 * 256)
assert report["constants"]["yaccel_q8"] == round(.44 * 256)
assert report["constants"]["run_jump_fwd_x_q8"] == 4 * 256
assert report["constants"]["run_jump_fwd_y_q8"] == round(-8.1 * 256)
assert report["constants"]["air_jump_neu_y_q8"] == round(-8.1 * 256)
assert report["constants"]["air_jump_num"] == 1
assert report["constants"]["air_jump_height"] == 35
assert report["constants"]["attack_dist"] == 160
assert report["constants"]["air_juggle"] == 15
assert report["constants"]["default_spark_no"] == 2
assert report["constants"]["default_guard_spark_no"] == 40
assert len(report["states"]) == 71
assert report["states"][0]["hitdef_count"] == 1
assert report["states"][0]["playsnd_count"] == 1
assert report["states"][0]["controller_count"] == 9
assert report["states"][0]["juggle"] == 5
assert report["states"][0]["has_juggle"] == 1
assert report["states"][0]["unsupported_controllers"] == []

hit = report["hitdefs"][0]
assert hit["trigger_kind"] == "IK_CNS_TRIGGER_ANIM_ELEM_EQ"
assert hit["trigger_value"] == 3
assert hit["damage"] == 23
assert hit["pause_p1"] == 8
assert hit["priority"] == 3
assert hit["priority_type"] == "IK_CNS_PRIORITY_HIT"
assert hit["ground_velocity_x_q8"] == -4 * 256
assert hit["guard_flags"] == "IK_CNS_GUARD_STAND | IK_CNS_GUARD_CROUCH | IK_CNS_GUARD_AIR"
assert hit["hit_flags"] == "IK_CNS_HIT_STAND | IK_CNS_HIT_CROUCH | IK_CNS_HIT_AIR | IK_CNS_HIT_FALL"
assert hit["guard_kill"] == 1
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
assert hit["down_hit_time"] == 22
assert hit["down_velocity_x_q8"] == -5 * 256
assert hit["down_velocity_y_q8"] == 0
assert hit["down_bounce"] == 1
assert hit["air_juggle"] == 2
assert hit["spark_no"] == 0
assert hit["guard_spark_no"] == 40

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

assert controllers[6]["type"] == "IK_CNS_CTRL_VAR_SET"
assert controllers[6]["value0"] == 7
assert controllers[6]["value1"] == 123456
assert controllers[7]["type"] == "IK_CNS_CTRL_VAR_ADD"
assert controllers[7]["value0"] == 7
assert controllers[7]["value1"] == 44

assert controllers[8]["type"] == "IK_CNS_CTRL_HELPER"
assert controllers[8]["value0"] == 0
assert report["helpers"] == [{
    "id": 77,
    "state_no": 1234,
    "pos_x_q8": 12 * 256,
    "pos_y_q8": -8 * 256,
    "facing": -1,
    "postype": "IK_CNS_HELPER_POS_P1",
    "keyctrl": 1,
    "ownpal": 1,
    "pause_move_time": 3,
    "super_move_time": 4,
}]
helper_state = next(
    row for row in report["states"] if row["number"] == 1234
)
helper_ctrl = report["controllers"][helper_state["controller_ofs"]]
assert helper_ctrl["type"] == "IK_CNS_CTRL_DESTROY_SELF"
assert helper_ctrl["trigger_kind"] == "IK_CNS_TRIGGER_TIME_EQ"
assert helper_ctrl["trigger_value"] == 5

proj_query_trigger = Section(
    "State 9000, Followup",
    [
        ("type", "ChangeState"),
        ("trigger1", "ProjHitTime = 0"),
        ("value", "9002"),
    ],
)
kind, value, packed = controller_trigger(
    proj_query_trigger, "changestate"
)
assert kind == "IK_CNS_TRIGGER_PROJECTILE_QUERY"
assert value == 0
assert "IK_CNS_PROJ_QUERY_HIT_TIME" in str(packed)
assert "IK_CNS_QUERY_EQ" in str(packed)

numproj_trigger = Section(
    "State 9000, Active projectile",
    [
        ("type", "CtrlSet"),
        ("trigger1", "NumProj > 0"),
        ("value", "1"),
    ],
)
kind, value, packed = controller_trigger(
    numproj_trigger, "ctrlset"
)
assert kind == "IK_CNS_TRIGGER_PROJECTILE_QUERY"
assert value == 0
assert "IK_CNS_PROJ_QUERY_NUM" in str(packed)
assert "IK_CNS_QUERY_GT" in str(packed)

projectile_ctrl = Section(
    "State 9000, Fireball",
    [
        ("type", "Projectile"),
        ("trigger1", "Time = 0"),
        ("projid", "42"),
        ("projanim", "9001"),
        ("projhitanim", "9002"),
        ("projremanim", "9003"),
        ("projcancelanim", "9004"),
        ("offset", "30,-20"),
        ("velocity", "4,-1"),
        ("velmul", "1,.98"),
        ("accel", "0,.1"),
        ("projremove", "0"),
        ("projremovetime", "90"),
        ("projhits", "3"),
        ("projmisstime", "8"),
        ("projpriority", "2"),
        ("projsprpriority", "4"),
        ("projedgebound", "50"),
        ("projstagebound", "60"),
        ("pausemovetime", "5"),
        ("supermovetime", "7"),
        ("bindtime", "2"),
        ("removeongethit", "1"),
        ("removeonchangestate", "1"),
        ("ownpal", "1"),
        ("attr", "S, SP"),
        ("damage", "40,5"),
        ("priority", "4, Hit"),
        ("pausetime", "6,8"),
        ("guardflag", "MA"),
        ("ground.type", "Low"),
        ("ground.slidetime", "12"),
        ("ground.hittime", "18"),
        ("ground.velocity", "-4,0"),
        ("air.velocity", "-3,-4"),
    ],
)
compiled_projectile = compile_projectile_controller(
    9000, projectile_ctrl, 2, 11, 2, 40
)
assert compiled_projectile is not None
proj_controller, projectile, projectile_hitdef = compiled_projectile
assert proj_controller["type"] == "IK_CNS_CTRL_PROJECTILE"
assert proj_controller["value0"] == 2
assert projectile["id"] == 42
assert projectile["anim_no"] == 9001
assert projectile["hit_anim_no"] == 9002
assert projectile["remove_anim_no"] == 9003
assert projectile["cancel_anim_no"] == 9004
assert projectile["hitdef_global"] == 11
assert projectile["hits"] == 3
assert projectile["miss_time"] == 8
assert projectile["priority"] == 2
assert projectile["remove_on_hit"] == 0
assert projectile["pause_move_time"] == 5
assert projectile["super_move_time"] == 7
assert projectile["postype"] == "IK_CNS_HELPER_POS_P1"
assert projectile["bind_time"] == 2
assert projectile["remove_on_gethit"] == 1
assert projectile["remove_on_state_change"] == 1
assert projectile_hitdef["damage"] == 40
assert projectile_hitdef["guard_damage"] == 5
assert projectile_hitdef["attack_attr_mask"] == "IK_CNS_ATTR_SPECIAL_PROJECTILE"

explod_ctrl = Section(
    "State 191, Wood",
    [
        ("type", "Explod"),
        ("trigger1", "AnimElemTime(7) = 1"),
        ("anim", "192"),
        ("postype", "p1"),
        ("pos", "60,-70"),
        ("velocity", "2,-4"),
        ("accel", "0,.32"),
        ("removetime", "35"),
        ("bindtime", "2"),
        ("removeongethit", "1"),
        ("removeonchangestate", "1"),
        ("facing", "-1"),
        ("vfacing", "-1"),
        ("scale", "1.5,.5"),
        ("trans", "addalpha"),
        ("alpha", "128,128"),
        ("persistent", "0"),
    ],
)
compiled_explod = compile_explod_controller(191, explod_ctrl, 3)
assert compiled_explod is not None
explod_controller, explod = compiled_explod
assert explod_controller["type"] == "IK_CNS_CTRL_EXPLOD"
assert explod_controller["value0"] == 3
assert explod_controller["value1"] == 1
assert explod["anim_no"] == 192
assert explod["pos_x_q8"] == 60 * 256
assert explod["pos_y_q8"] == -70 * 256
assert explod["vel_x_q8"] == 2 * 256
assert explod["vel_y_q8"] == -4 * 256
assert explod["accel_x_q8"] == 0
assert explod["accel_y_q8"] == round(.32 * 256)
assert explod["remove_time"] == 35
assert explod["bind_time"] == 2
assert explod["remove_on_gethit"] == 1
assert explod["remove_on_state_change"] == 1
assert explod["facing"] == -1
assert explod["vfacing"] == -1
assert explod["scale_x_q8"] == round(1.5 * 256)
assert explod["scale_y_q8"] == round(.5 * 256)
assert explod["trans_mode"] == "IK_CNS_TRANS_ALPHA"
assert explod["alpha"] == 128

p2_explod_ctrl = Section(
    "State 191, P2 Explod",
    [
        ("type", "Explod"),
        ("trigger1", "1"),
        ("anim", "192"),
        ("postype", "p2"),
        ("pos", "10,-5"),
    ],
)
compiled_p2_explod = compile_explod_controller(191, p2_explod_ctrl, 4)
assert compiled_p2_explod is not None
_, p2_explod = compiled_p2_explod
assert p2_explod["postype"] == "IK_CNS_HELPER_POS_P2"

p2_projectile_ctrl = Section(
    "State 9000, P2 Projectile",
    [
        ("type", "Projectile"),
        ("trigger1", "1"),
        ("postype", "p2"),
        ("projanim", "9001"),
        ("attr", "S, NP"),
        ("damage", "1,0"),
    ],
)
compiled_p2_projectile = compile_projectile_controller(
    9000, p2_projectile_ctrl, 3, 12, 2, 40
)
assert compiled_p2_projectile is not None
_, p2_projectile, _ = compiled_p2_projectile
assert p2_projectile["postype"] == "IK_CNS_HELPER_POS_P2"

unsupported_helper = Section(
    "State 0, Unsupported Helper",
    [
        ("type", "Helper"),
        ("trigger1", "1"),
        ("stateno", "1234"),
        ("size.xscale", "2"),
    ],
)
assert compile_helper_controller(0, unsupported_helper, 0) is None

print("ikemen CNS compiler: OK")

source_rows = {row["number"]: row for row in report["states"][:25]}
throw_rows = source_rows
common_rows = {row["number"]: row for row in report["states"][25:]}
assert throw_rows[800]["unsupported_controllers"] == []
assert throw_rows[810]["unsupported_controllers"] == []
assert throw_rows[820]["unsupported_controllers"] == []
assert throw_rows[821]["unsupported_controllers"] == []
assert throw_rows[810]["controller_count"] == 13
assert throw_rows[820]["controller_count"] == 2
assert throw_rows[821]["controller_count"] == 4

throw_hit = report["hitdefs"][1]
assert throw_hit["flags"] == "IK_CNS_HITDEF_FALL | IK_CNS_HITDEF_THROW"
assert throw_hit["hit_flags"] == "IK_CNS_HIT_STAND | IK_CNS_HIT_CROUCH | IK_CNS_HIT_NOT_GETHIT"
assert throw_hit["priority"] == 1
assert throw_hit["priority_type"] == "IK_CNS_PRIORITY_MISS"
assert throw_hit["p1_state_no"] == 810
assert throw_hit["p2_state_no"] == 820
assert throw_hit["p1_facing"] == 1
assert throw_hit["p2_facing"] == 1
assert throw_hit["p1_spr_priority"] == 1

assert source_rows[1000]["unsupported_controllers"] == []
assert source_rows[1000]["hitdef_count"] == 2
assert source_rows[1000]["controller_count"] == 5
assert source_rows[1000]["power_add"] == 55
assert source_rows[1010]["unsupported_controllers"] == []
assert source_rows[1010]["hitdef_count"] == 2
assert source_rows[1010]["controller_count"] == 5
assert source_rows[1010]["power_add"] == 60

palm_near = report["hitdefs"][2]
palm_far = report["hitdefs"][3]
assert palm_near["damage"] == 90
assert palm_near["priority"] == 5
assert palm_near["flags"] == "IK_CNS_HITDEF_FALL"
assert palm_near["p2_body_dist_op"] == "IK_CNS_P2_DIST_LT"
assert palm_near["p2_body_dist_x"] == 40
assert palm_far["damage"] == 85
assert palm_far["priority"] == 4
assert palm_far["flags"] == "0u"
assert palm_far["p2_body_dist_op"] == "IK_CNS_P2_DIST_GE"
assert palm_far["p2_body_dist_x"] == 40

palm1000_ctrls = [
    c for c in report["controllers"] if c["state_number"] == 1000
]
assert palm1000_ctrls[1]["trigger_kind"] == "IK_CNS_TRIGGER_ANIM_ELEM_EQ_OR"
assert palm1000_ctrls[1]["trigger_value"] == 3
assert palm1000_ctrls[1]["trigger_value2"] == 13

palm1010_ctrls = [
    c for c in report["controllers"] if c["state_number"] == 1010
]
vel5 = next(c for c in palm1010_ctrls if c["type"] == "IK_CNS_CTRL_VEL_SET")
assert vel5["value0"] == 4 * 256
assert "IK_CNS_CTRL_AXIS_X" in vel5["flags"]
assert "IK_CNS_CTRL_LOCAL_X" in vel5["flags"]

assert source_rows[1020]["power_add"] == -330
assert source_rows[1020]["juggle"] == 6
assert source_rows[1020]["controller_count"] == 6
assert source_rows[1020]["unsupported_controllers"] == [
    "afterimage", "afterimagetime", "palfx"
]
fast_hit = report["hitdefs"][6]
assert fast_hit["damage"] == 95
assert fast_hit["guard_damage"] == 5
assert fast_hit["p2_state_no"] == 1025
assert fast_hit["p2_facing"] == 1
assert fast_hit["flags"] == "IK_CNS_HITDEF_FALL"

fast_ctrls = [
    c for c in report["controllers"] if c["state_number"] == 1020
]
friction = next(
    c for c in fast_ctrls
    if c["type"] == "IK_CNS_CTRL_VEL_MUL_X_BY_ANIM_ELEM"
)
assert friction["trigger_value"] == 6
assert friction["value0"] == round(.85 * 256)
assert friction["value1"] == round(.85 * .8 * 256)

assert source_rows[1025]["anim"] == -1
assert source_rows[1025]["controller_count"] == 2
assert source_rows[1025]["unsupported_controllers"] == []
state1025_ctrls = [
    c for c in report["controllers"] if c["state_number"] == 1025
]
assert state1025_ctrls[1]["trigger_kind"] == "IK_CNS_TRIGGER_HIT_SHAKE_OVER"

assert source_rows[1026]["anim"] == -1
assert source_rows[1026]["controller_count"] == 4
assert source_rows[1026]["owns_air_accel"] == 1
assert source_rows[1026]["unsupported_controllers"] == ["screenbound"]
state1026_ctrls = [
    c for c in report["controllers"] if c["state_number"] == 1026
]
hitvel = next(
    c for c in state1026_ctrls if c["type"] == "IK_CNS_CTRL_HIT_VEL_SET"
)
assert hitvel["trigger_kind"] == "IK_CNS_TRIGGER_TIME_EQ"
assert hitvel["trigger_value"] == 1
assert "IK_CNS_CTRL_AXIS_X" in hitvel["flags"]
assert "IK_CNS_CTRL_AXIS_Y" in hitvel["flags"]
wall_branch = next(
    c for c in state1026_ctrls
    if c["type"] == "IK_CNS_CTRL_CHANGE_STATE"
)
assert wall_branch["trigger_kind"] == "IK_CNS_TRIGGER_AIR_NEAR_BODY_EDGE"
assert wall_branch["trigger_value"] == -15 * 256
assert wall_branch["trigger_value2"] == 20
assert wall_branch["value0"] == 1027

assert source_rows[1027]["anim"] == -1
assert source_rows[1027]["controller_count"] == 5
assert source_rows[1027]["playsnd_count"] == 1
assert source_rows[1027]["unsupported_controllers"] == [
    "explod", "screenbound"
]
state1027_ctrls = [
    c for c in report["controllers"] if c["state_number"] == 1027
]
turn1027 = next(c for c in state1027_ctrls if c["type"] == "IK_CNS_CTRL_TURN")
assert turn1027["trigger_kind"] == "IK_CNS_TRIGGER_STATE_ENTRY_FRONT_EDGE_BODY_LE"
pos1027 = next(
    c for c in state1027_ctrls
    if c["type"] == "IK_CNS_CTRL_POS_ADD_FROM_BACK_EDGE"
)
assert pos1027["trigger_kind"] == "IK_CNS_TRIGGER_TIME_EQ"
assert pos1027["trigger_value"] == 1
assert pos1027["value0"] == 15 * 256
freeze1027 = next(
    c for c in state1027_ctrls if c["type"] == "IK_CNS_CTRL_POS_FREEZE"
)
assert "IK_CNS_CTRL_AXIS_X" in freeze1027["flags"]
assert "IK_CNS_CTRL_AXIS_Y" in freeze1027["flags"]

assert source_rows[1028]["anim"] == -1
assert source_rows[1028]["controller_count"] == 5
assert source_rows[1028]["owns_air_accel"] == 1
assert source_rows[1028]["unsupported_controllers"] == [
    "changeanim", "nothitby"
]
state1028_ctrls = [
    c for c in report["controllers"] if c["state_number"] == 1028
]
velsets1028 = [
    c for c in state1028_ctrls if c["type"] == "IK_CNS_CTRL_VEL_SET"
]
assert len(velsets1028) == 2
assert all(c["trigger_value"] == 1 for c in velsets1028)
assert any(c["value1"] == -6 * 256 for c in velsets1028)
assert any(c["value0"] == round(1.6 * 256) for c in velsets1028)
turn1028 = next(c for c in state1028_ctrls if c["type"] == "IK_CNS_CTRL_TURN")
assert turn1028["trigger_kind"] == "IK_CNS_TRIGGER_STATE_ENTRY_BACK_EDGE_LT"

assert source_rows[1050]["power_add"] == 55
assert source_rows[1050]["hitdef_count"] == 1
assert source_rows[1050]["controller_count"] == 2
assert source_rows[1050]["unsupported_controllers"] == ["null"]
assert source_rows[1051]["hitdef_persist"] == 1
assert source_rows[1051]["has_velset"] == 1
assert source_rows[1051]["velset_x_q8"] == 2 * 256
assert source_rows[1051]["velset_y_q8"] == -6 * 256
assert source_rows[1051]["controller_count"] == 3
assert source_rows[1051]["owns_air_accel"] == 1
assert source_rows[1051]["unsupported_controllers"] == []
knee1051 = [
    c for c in report["controllers"] if c["state_number"] == 1051
]
kick_branch = next(c for c in knee1051 if c["value0"] == 1055)
assert kick_branch["trigger_kind"] == "IK_CNS_TRIGGER_COMMAND_ANY_VY_LT_Q8"
assert kick_branch["trigger_value"] == (1 << 5) | (1 << 6)
assert kick_branch["trigger_value2"] == -256
land_branch = next(c for c in knee1051 if c["value0"] == 1052)
assert land_branch["trigger_kind"] == "IK_CNS_TRIGGER_VY_GT_Q8_AT_LEVEL"
assert land_branch["trigger_value"] == -10 * 256
assert land_branch["trigger_value2"] == 0

assert source_rows[1052]["controller_count"] == 3
assert source_rows[1052]["playsnd_count"] == 1
assert source_rows[1052]["unsupported_controllers"] == ["ctrlset"]
ground1052 = next(
    c for c in report["controllers"]
    if c["state_number"] == 1052 and c["type"] == "IK_CNS_CTRL_POS_SET"
)
assert ground1052["trigger_value"] == 1
assert ground1052["value1"] == 0
assert "IK_CNS_CTRL_AXIS_Y" in ground1052["flags"]

assert source_rows[1055]["hitdef_count"] == 1
assert source_rows[1055]["controller_count"] == 3
assert source_rows[1055]["owns_air_accel"] == 1
assert source_rows[1055]["unsupported_controllers"] == []
kick_hit = next(
    h for h in report["hitdefs"] if h["state_number"] == 1055
)
assert kick_hit["damage"] == 35
assert kick_hit["alt_damage"] == 40
assert kick_hit["alt_damage_prev_state"] == 1061
assert kick_hit["has_alt_damage"] == 1
assert kick_hit["spark_no"] == 2
assert kick_hit["guard_spark_no"] == 40
assert "IK_CNS_HITDEF_AIR_FALL" in kick_hit["flags"]

assert source_rows[1056]["controller_count"] == 2
assert source_rows[1056]["unsupported_controllers"] == []

assert source_rows[1060]["power_add"] == 60
assert source_rows[1060]["hitdef_count"] == 1
assert source_rows[1060]["controller_count"] == 3
assert source_rows[1060]["unsupported_controllers"] == []
assert source_rows[1061]["hitdef_persist"] == 1
assert source_rows[1061]["controller_count"] == 3
assert source_rows[1061]["owns_air_accel"] == 1
assert source_rows[1061]["unsupported_controllers"] == []

for upper in (1100, 1110):
    assert source_rows[upper]["hitdef_count"] == 2
    assert source_rows[upper]["controller_count"] == 2
    assert source_rows[upper]["playsnd_count"] == 1
    assert source_rows[upper]["unsupported_controllers"] == []

upper1100 = [
    h for h in report["hitdefs"] if h["state_number"] == 1100
]
assert len(upper1100) == 2
assert upper1100[0]["damage"] == 52
assert "IK_CNS_HITDEF_FORCE_STAND" in upper1100[0]["flags"]
assert upper1100[0]["p2_facing"] == 1
assert upper1100[1]["damage"] == 55
assert "IK_CNS_HITDEF_FALL" in upper1100[1]["flags"]
assert upper1100[1]["fall_recover_time"] == 40
assert upper1100[1]["yaccel_q8"] == round(.4 * 256)

upper1110 = [
    h for h in report["hitdefs"] if h["state_number"] == 1110
]
assert upper1110[0]["damage"] == 57
assert upper1110[1]["damage"] == 60
assert upper1110[1]["fall_recover_time"] == 50
assert upper1110[1]["yaccel_q8"] == round(.4 * 256)

assert source_rows[1120]["power_add"] == -330
assert source_rows[1120]["juggle"] == 6
assert source_rows[1120]["hitdef_count"] == 2
assert source_rows[1120]["controller_count"] == 2
assert source_rows[1120]["unsupported_controllers"] == [
    "afterimage", "afterimagetime", "palfx"
]
upper1120 = [
    h for h in report["hitdefs"] if h["state_number"] == 1120
]
assert upper1120[0]["damage"] == 30
assert upper1120[0]["trigger_kind"] == "IK_CNS_TRIGGER_TIME_EQ"
assert upper1120[0]["trigger_value"] == 0
assert upper1120[0]["has_trigger2"] == 1
assert upper1120[0]["trigger2_kind"] == "IK_CNS_TRIGGER_ANIM_ELEM_EQ"
assert upper1120[0]["trigger2_value"] == 4
assert upper1120[0]["spark_y"] == -48
assert upper1120[0]["trigger2_spark_y"] == -55
assert "IK_CNS_HITDEF_FORCE_STAND" in upper1120[0]["flags"]
assert upper1120[1]["damage"] == 68
assert upper1120[1]["fall_recover_time"] == 60
assert upper1120[1]["yaccel_q8"] == round(.4 * 256)

for blow in (1200, 1210):
    assert source_rows[blow]["hitdef_count"] == 1
    assert source_rows[blow]["controller_count"] == 3
    assert source_rows[blow]["playsnd_count"] == 1
    assert source_rows[blow]["unsupported_controllers"] == ["envshake"]

blow1200 = next(
    h for h in report["hitdefs"] if h["state_number"] == 1200
)
assert blow1200["damage"] == 100
assert blow1200["ground_cornerpush_veloff_q8"] == -12 * 256
blow1210 = next(
    h for h in report["hitdefs"] if h["state_number"] == 1210
)
assert blow1210["damage"] == 125
assert blow1210["ground_cornerpush_veloff_q8"] == -15 * 256

assert source_rows[1220]["power_add"] == -330
assert source_rows[1220]["juggle"] == 6
assert source_rows[1220]["hitdef_count"] == 1
assert source_rows[1220]["controller_count"] == 3
assert source_rows[1220]["unsupported_controllers"] == [
    "afterimage", "afterimagetime", "envshake", "palfx"
]
blow1220 = next(
    h for h in report["hitdefs"] if h["state_number"] == 1220
)
assert blow1220["damage"] == 125
assert blow1220["ground_cornerpush_veloff_q8"] == -20 * 256
assert "IK_CNS_HITDEF_AIR_FALL" in blow1220["flags"]
assert blow1220["yaccel_q8"] == round(.4 * 256)

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
assert common_rows[5110]["controller_count"] == 9
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
assert common_rows[5100]["controller_count"] == 7
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
state5210_ctrls = [
    c for c in report["controllers"] if c["state_number"] == 5210
]
assert any(
    c["type"] == "IK_CNS_CTRL_PAL_FX"
    for c in state5210_ctrls
)
turn5210 = next(
    c for c in state5210_ctrls
    if c["type"] == "IK_CNS_CTRL_TURN"
)
assert turn5210["trigger_kind"] == (
    "IK_CNS_TRIGGER_P2_DIST_X_LT_Q8_AT_TIME"
)
assert turn5210["trigger_value"] == -20 * 256
assert any(
    c["type"] == "IK_CNS_CTRL_NOT_HIT_BY" and
    c["value0"] == 7 and c["value1"] == 15
    for c in state5210_ctrls
)
assert common_rows[5210]["controller_count"] == 10
assert common_rows[100]["assert_special_flags"] == (
    "IK_CNS_STATE_ASSERT_NO_WALK | "
    "IK_CNS_STATE_ASSERT_NO_AUTO_TURN"
)
assert 100 not in report["common_deferred"]
assert 5210 not in report["common_deferred"]
assert report["common_deferred"][150] == ["ForceFeedback"]
assert report["constants"]["liedown_time"] == 60
assert report["constants"]["air_gethit_groundlevel_q8"] == 25 * 256
assert report["constants"]["air_gethit_trip_groundlevel_q8"] == 15 * 256
assert report["constants"]["down_bounce_offset_y_q8"] == 20 * 256
assert report["constants"]["down_bounce_yaccel_q8"] == round(.4 * 256)
assert report["constants"]["down_bounce_groundlevel_q8"] == 12 * 256

super_rows = {row["number"]: row for row in super_report["states"]}
assert set(super_rows) == {3000, 3050, 3051}
assert super_rows[3000]["juggle"] == 4
assert super_rows[3000]["unsupported_controllers"] == []
assert super_rows[3050]["unsupported_controllers"] == []
assert super_rows[3051]["unsupported_controllers"] == []
super3000 = [
    c for c in super_report["controllers"]
    if c["state_number"] == 3000
]
after3000 = next(
    c for c in super3000 if c["type"] == "IK_CNS_CTRL_AFTER_IMAGE"
)
assert after3000["value0"] == 2
assert after3000["value1"] == 20
assert after3000["value2"] == 1
assert after3000["value3"] == 4
assert (after3000["value4"] & 0xff) == 30
assert ((after3000["value4"] >> 8) & 0xff) == 30
assert ((after3000["value4"] >> 16) & 0xff) == 30
assert (after3000["value5"] & 0xff) == 120
assert ((after3000["value5"] >> 8) & 0xff) == 120
assert ((after3000["value5"] >> 16) & 0xff) == 220
assert (after3000["value6"] & 0xff) == 10
assert ((after3000["value6"] >> 8) & 0xff) == 10
assert ((after3000["value6"] >> 16) & 0xff) == 25
assert (after3000["value7"] & 0xff) == round(.65 * 255)
assert ((after3000["value7"] >> 8) & 0xff) == round(.65 * 255)
assert ((after3000["value7"] >> 16) & 0xff) == round(.75 * 255)
superpause3000 = next(
    c for c in super3000 if c["type"] == "IK_CNS_CTRL_SUPER_PAUSE"
)
assert superpause3000["trigger_kind"] == (
    "IK_CNS_TRIGGER_ANIM_ELEM_TIME_EQ_PACKED"
)
assert superpause3000["value0"] == 30
assert superpause3000["value1"] == -1000
super3000_sounds = [
    p for p in super_report["playsnds"]
    if p["state_number"] == 3000
]
assert len(super3000_sounds) == 4
assert any(
    p["group"] == 20 and p["item"] == 0
    for p in super3000_sounds
)
assert sum(
    1 for p in super3000_sounds
    if p["group"] == 0 and p["item"] == 3
) == 3
invuln3000 = [
    c for c in super3000 if c["type"] == "IK_CNS_CTRL_NOT_HIT_BY"
]
assert len(invuln3000) == 2
assert "IK_CNS_ATTR_NORMAL_ATTACK" in str(invuln3000[0]["value0"])
assert "IK_CNS_ATTR_SPECIAL_ATTACK" in str(invuln3000[0]["value0"])
assert "IK_CNS_ATTR_NORMAL_THROW" in str(invuln3000[0]["value0"])
steps3000 = next(
    c for c in super3000 if c["type"] == "IK_CNS_CTRL_POS_ADD"
)
assert steps3000["trigger_kind"] == "IK_CNS_TRIGGER_ANIM_ELEM_MASK"
mask3000 = (
    (1 << 2) | (1 << 10) | (1 << 12) |
    (1 << 18) | (1 << 20) | (1 << 30)
)
assert (steps3000["trigger_value"] & 0xffff) == (mask3000 & 0xffff)
assert (steps3000["trigger_value2"] & 0xffff) == ((mask3000 >> 16) & 0xffff)
super3050 = [
    c for c in super_report["controllers"]
    if c["state_number"] == 3050
]
success3050 = next(
    c for c in super3050 if c["type"] == "IK_CNS_CTRL_CHANGE_STATE"
)
assert success3050["trigger_kind"] == "IK_CNS_TRIGGER_MOVE_HIT"
assert success3050["value0"] == 3051
assert super_report["hitdefs"][0]["attack_attr_mask"] == "IK_CNS_ATTR_HYPER_ATTACK"
assert super_report["hitdefs"][2]["attack_attr_mask"] == "IK_CNS_ATTR_HYPER_ATTACK"
assert super_report["hitdefs"][2]["fall_damage"] == 70
assert super_report["hitdefs"][2]["envshake_time"] == 25
assert super_report["hitdefs"][2]["envshake_ampl"] == 7
assert super_report["hitdefs"][2]["envshake_freq"] == 176
assert super_report["hitdefs"][2]["fall_envshake_time"] == 15
assert super_report["hitdefs"][2]["fall_envshake_ampl"] == 6
assert super_report["hitdefs"][2]["fall_envshake_freq"] == 178

assert len(zankou_report["states"]) == 2
zankou_rows = {row["number"]: row for row in zankou_report["states"]}
assert zankou_rows[1400]["power_add"] == 50
assert zankou_rows[1400]["juggle"] == 4
assert zankou_rows[1420]["power_add"] == -330
assert zankou_rows[1420]["juggle"] == 6
assert zankou_rows[1420]["unsupported_controllers"] == []
zankou_hits = zankou_report["hitdefs"]
assert zankou_hits[0]["attack_attr_mask"] == "IK_CNS_ATTR_SPECIAL_ATTACK"
assert zankou_hits[1]["trigger_kind"] == (
    "IK_CNS_TRIGGER_ANIM_ELEM_TIME_EQ_PACKED"
)
assert zankou_hits[1]["trigger_value"] == ((4 << 8) | 0xfe)
assert zankou_hits[1]["damage"] == 25
assert zankou_hits[2]["trigger_kind"] == "IK_CNS_TRIGGER_ANIM_ELEM_EQ"
assert zankou_hits[2]["trigger_value"] == 4
assert zankou_hits[2]["damage"] == 100
zankou_1400 = [
    c for c in zankou_report["controllers"]
    if c["state_number"] == 1400
]
step1400 = next(c for c in zankou_1400 if c["type"] == "IK_CNS_CTRL_POS_ADD")
assert step1400["trigger_kind"] == "IK_CNS_TRIGGER_ANIM_ELEM_MASK"
assert step1400["trigger_value"] == (
    (1 << (2 - 1)) | (1 << (3 - 1)) | (1 << (4 - 1))
)
zankou_1420 = [
    c for c in zankou_report["controllers"]
    if c["state_number"] == 1420
]
after1420 = next(
    c for c in zankou_1420 if c["type"] == "IK_CNS_CTRL_AFTER_IMAGE"
)
assert after1420["value0"] == 2
assert after1420["value1"] == 13
assert after1420["value2"] == 1
assert after1420["value3"] == 2
assert (after1420["value4"] & 0xff) == 30
assert ((after1420["value4"] >> 8) & 0xff) == 30
assert (after1420["value5"] & 0xff) == 70
assert ((after1420["value5"] >> 8) & 0xff) == 70
assert ((after1420["value5"] >> 16) & 0xff) == 20
assert (after1420["value6"] & 0xff) == (256 - 10)
assert (after1420["value7"] & 0xff) == round(.85 * 255)
assert ((after1420["value7"] >> 16) & 0xff) == round(.50 * 255)
aftertime1420 = next(
    c for c in zankou_1420 if c["type"] == "IK_CNS_CTRL_AFTER_IMAGE_TIME"
)
assert aftertime1420["trigger_kind"] == "IK_CNS_TRIGGER_ANIM_ELEM_BEFORE"
assert aftertime1420["trigger_value"] == 8
palfx1420 = next(
    c for c in zankou_1420 if c["type"] == "IK_CNS_CTRL_PAL_FX"
)
assert palfx1420["value0"] == 20
assert (palfx1420["value1"] & 0x1ff) == 32
assert ((palfx1420["value1"] >> 9) & 0x1ff) == 16
assert ((palfx1420["value1"] >> 18) & 0x1ff) == 0
assert (palfx1420["value2"] & 0x1ff) == 64
assert ((palfx1420["value2"] >> 9) & 0x1ff) == 32
assert ((palfx1420["value2"] >> 18) & 0x1ff) == 5
assert palfx1420["value3"] == 3
assert (palfx1420["value4"] & 0x1ff) == 256
assert ((palfx1420["value4"] >> 9) & 0x1ff) == 192
assert ((palfx1420["value4"] >> 18) & 0x1ff) == 128
assert (palfx1420["value5"] & 0x1ff) == 0
assert ((palfx1420["value5"] >> 9) & 0x1ff) == ((-64) & 0x1ff)
assert ((palfx1420["value5"] >> 18) & 0x1ff) == ((-128) & 0x1ff)
assert palfx1420["value6"] == 5
dash1420 = next(c for c in zankou_1420 if c["type"] == "IK_CNS_CTRL_VEL_SET")
assert dash1420["trigger_kind"] == "IK_CNS_TRIGGER_ANIM_ELEM_TIME_RANGE"
assert dash1420["trigger_value"] == 3
assert dash1420["value0"] == 20 * 256

assert len(reversal_report["reversals"]) == 2
high_reversal = reversal_report["reversals"][0]
assert high_reversal["state_number"] == 1300
assert high_reversal["start_time"] == 0
assert high_reversal["end_time"] == 8
assert high_reversal["attacker_state_mask"] == (
    "IK_CNS_REVERSAL_STATE_STAND | IK_CNS_REVERSAL_STATE_AIR"
)
assert high_reversal["incoming_attr_mask"] == (
    "IK_CNS_ATTR_NORMAL_ATTACK | "
    "IK_CNS_ATTR_SPECIAL_ATTACK | "
    "IK_CNS_ATTR_HYPER_ATTACK"
)
assert high_reversal["p1_state_no"] == 1310
assert high_reversal["spark_no"] == 40
assert high_reversal["p1_spr_priority"] == 2
assert high_reversal["p2_spr_priority"] == 1
assert len(reversal_report["hitoverrides"]) == 1
high_override = reversal_report["hitoverrides"][0]
assert high_override["state_number"] == 1300
assert high_override["start_time"] == 0
assert high_override["end_time"] == 8
assert high_override["self_state_mask"] == (
    "IK_CNS_REVERSAL_STATE_STAND | IK_CNS_REVERSAL_STATE_AIR"
)
assert high_override["incoming_attr_mask"] == (
    "IK_CNS_ATTR_NORMAL_PROJECTILE | "
    "IK_CNS_ATTR_SPECIAL_PROJECTILE | "
    "IK_CNS_ATTR_HYPER_PROJECTILE"
)
assert high_override["target_state"] == 1310
blocking_rows = {
    row["number"]: row for row in reversal_report["states"]
}
assert blocking_rows[1300]["reversal_count"] == 1
assert blocking_rows[1300]["hitoverride_count"] == 1
assert blocking_rows[1340]["reversal_count"] == 1
assert blocking_rows[1310]["unsupported_controllers"] == []
blocked1310 = [
    c for c in reversal_report["controllers"]
    if c["state_number"] == 1310
]
pause1310 = next(c for c in blocked1310 if c["type"] == "IK_CNS_CTRL_PAUSE")
assert pause1310["trigger_kind"] == "IK_CNS_TRIGGER_TIME_EQ"
assert pause1310["trigger_value"] == 1
assert pause1310["value0"] == 20
nothit1310 = next(
    c for c in blocked1310 if c["type"] == "IK_CNS_CTRL_NOT_HIT_BY"
)
assert nothit1310["value0"] == (
    "IK_CNS_REVERSAL_STATE_STAND | IK_CNS_REVERSAL_STATE_CROUCH | "
    "IK_CNS_REVERSAL_STATE_AIR"
)
assert nothit1310["value1"] == 1
blocking_ctrls = reversal_report["controllers"]
width1300 = next(
    c for c in blocking_ctrls
    if c["state_number"] == 1300 and c["type"] == "IK_CNS_CTRL_WIDTH"
)
assert width1300["trigger_kind"] == "IK_CNS_TRIGGER_ANIM_ELEM_BEFORE"
gravity1340 = next(
    c for c in blocking_ctrls
    if c["state_number"] == 1340 and c["type"] == "IK_CNS_CTRL_VEL_ADD"
)
assert "IK_CNS_CTRL_USE_YACCEL" in gravity1340["flags"]
freeze1350 = next(
    c for c in blocking_ctrls
    if c["state_number"] == 1350 and c["type"] == "IK_CNS_CTRL_POS_FREEZE"
)
assert freeze1350["trigger_kind"] == "IK_CNS_TRIGGER_ANIM_ELEM_BEFORE"
assert "IK_CNS_CTRL_AXIS_X" in freeze1350["flags"]
assert "IK_CNS_CTRL_AXIS_Y" in freeze1350["flags"]

assert report["constants"]["air_gethit_groundrecover_x_q8"] == round(-.15 * 256)
assert report["constants"]["air_gethit_groundrecover_y_q8"] == round(-3.5 * 256)
assert report["constants"]["air_gethit_groundrecover_threshold_q8"] == -20 * 256
assert report["constants"]["air_gethit_groundrecover_groundlevel_q8"] == 10 * 256
assert report["constants"]["air_gethit_airrecover_mul_x_q8"] == round(.5 * 256)
assert report["constants"]["air_gethit_airrecover_add_y_q8"] == round(-4.5 * 256)
assert report["constants"]["air_gethit_airrecover_threshold_q8"] == -256
assert report["constants"]["air_gethit_airrecover_yaccel_q8"] == round(.35 * 256)
