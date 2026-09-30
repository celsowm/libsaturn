#!/usr/bin/env python3
"""Compile the deterministic KFM CNS subset used by the Saturn runtime.

The Saturn never parses CNS text. This offline compiler emits:
* character constants ([Data]/[Size]/[Velocity]/[Movement]);
* selected Statedef metadata;
* HitDef and PlaySnd records;
* compact runtime controllers.

Unsupported controllers/triggers are reported instead of approximated.
"""

from __future__ import annotations

import argparse
import json
import re
from dataclasses import dataclass, field
from pathlib import Path

Q8 = 256

STATE_TYPE = {
    "S": "IK_CNS_STATE_STAND",
    "C": "IK_CNS_STATE_CROUCH",
    "A": "IK_CNS_STATE_AIR",
    "L": "IK_CNS_STATE_LIEDOWN",
    "U": "IK_CNS_STATE_UNCHANGED",
}

MOVE_TYPE = {
    "I": "IK_CNS_MOVE_IDLE",
    "A": "IK_CNS_MOVE_ATTACK",
    "H": "IK_CNS_MOVE_HIT",
    "U": "IK_CNS_MOVE_UNCHANGED",
}

PHYSICS = {
    "N": "IK_CNS_PHYS_NONE",
    "S": "IK_CNS_PHYS_STAND",
    "C": "IK_CNS_PHYS_CROUCH",
    "A": "IK_CNS_PHYS_AIR",
    "U": "IK_CNS_PHYS_NONE",
}

GROUND_TYPE = {
    "normal": "IK_CNS_GROUND_NORMAL",
    "high": "IK_CNS_GROUND_HIGH",
    "low": "IK_CNS_GROUND_LOW",
    "trip": "IK_CNS_GROUND_TRIP",
}


@dataclass
class Section:
    name: str
    values: list[tuple[str, str]] = field(default_factory=list)

    def all(self, key: str) -> list[str]:
        key = key.lower()
        return [value for k, value in self.values if k.lower() == key]

    def get(self, key: str, default: str | None = None) -> str | None:
        values = self.all(key)
        return values[-1] if values else default


@dataclass
class State:
    number: int
    statedef: Section
    controllers: list[Section]


def strip_comment(line: str) -> str:
    return line.split(";", 1)[0].strip()


def parse_sections(path: Path) -> list[Section]:
    sections: list[Section] = []
    current: Section | None = None
    for raw in path.read_text(encoding="utf-8-sig", errors="strict").splitlines():
        line = strip_comment(raw)
        if not line:
            continue
        if line.startswith("[") and line.endswith("]"):
            current = Section(line[1:-1].strip())
            sections.append(current)
            continue
        if current is not None and "=" in line:
            key, value = line.split("=", 1)
            current.values.append((key.strip(), value.strip()))
    return sections


def collect_states(
    sections: list[Section],
) -> tuple[dict[str, Section], list[State]]:
    globals_: dict[str, Section] = {}
    states: list[State] = []
    current: State | None = None

    for section in sections:
        m = re.fullmatch(r"Statedef\s+(-?\d+)", section.name, flags=re.I)
        if m:
            current = State(int(m.group(1)), section, [])
            states.append(current)
        elif re.match(r"State\s+", section.name, flags=re.I):
            if current is not None:
                current.controllers.append(section)
        else:
            globals_[section.name.lower()] = section

    return globals_, states


def number(text: str | None, default: float = 0.0) -> float:
    if text is None:
        return default
    value = text.strip()
    if not re.fullmatch(r"[+-]?(?:\d+(?:\.\d*)?|\.\d+)", value):
        raise ValueError(f"expected numeric literal, got {text!r}")
    return float(value)


def integer(text: str | None, default: int = 0) -> int:
    value = number(text, float(default))
    if int(value) != value:
        raise ValueError(f"expected integer literal, got {text!r}")
    return int(value)


def pair(
    text: str | None,
    default_x: float = 0.0,
    default_y: float = 0.0,
) -> tuple[float, float]:
    if text is None:
        return default_x, default_y

    parts = [p.strip() for p in text.split(",")]
    if len(parts) == 1:
        return number(parts[0]), default_y
    if len(parts) != 2:
        raise ValueError(f"expected scalar or pair, got {text!r}")
    return number(parts[0]), number(parts[1])


def q8(value: float) -> int:
    result = int(round(value * Q8))
    if not -32768 <= result <= 32767:
        raise ValueError(f"Q8.8 overflow for {value}")
    return result


def anim_type_code(text: str | None) -> int:
    value = (text or "light").strip().lower()
    mapping = {
        "light": 0,
        "medium": 1,
        "med": 1,
        "hard": 2,
        "heavy": 2,
        "back": 3,
        "up": 4,
        "diagup": 5,
        "diag-up": 5,
    }
    return mapping.get(value, 0)


def priority_type_code(text: str | None) -> str:
    value = (text or "4, Hit").split(",", 1)
    kind = value[1].strip().lower() if len(value) > 1 else "hit"
    if kind in ("miss", "m"):
        return "IK_CNS_PRIORITY_MISS"
    if kind in ("dodge", "d"):
        return "IK_CNS_PRIORITY_DODGE"
    return "IK_CNS_PRIORITY_HIT"


def guard_mask(text: str | None) -> str:
    value = (text or "").strip().upper()
    bits: list[str] = []
    if "H" in value:
        bits.append("IK_CNS_GUARD_STAND")
    if "L" in value:
        bits.append("IK_CNS_GUARD_CROUCH")
    if "M" in value:
        bits += ["IK_CNS_GUARD_STAND", "IK_CNS_GUARD_CROUCH"]
    if "A" in value:
        bits.append("IK_CNS_GUARD_AIR")
    return " | ".join(dict.fromkeys(bits)) if bits else "0u"


def hit_mask(text: str | None) -> str:
    value = (text or "MAF").strip().upper()
    bits: list[str] = []
    if "H" in value:
        bits.append("IK_CNS_HIT_STAND")
    if "L" in value:
        bits.append("IK_CNS_HIT_CROUCH")
    if "M" in value:
        bits += ["IK_CNS_HIT_STAND", "IK_CNS_HIT_CROUCH"]
    if "A" in value:
        bits.append("IK_CNS_HIT_AIR")
    if "F" in value:
        bits.append("IK_CNS_HIT_FALL")
    if "D" in value:
        bits.append("IK_CNS_HIT_DOWN")
    if "+" in value:
        bits.append("IK_CNS_HIT_ONLY_GETHIT")
    if "-" in value:
        bits.append("IK_CNS_HIT_NOT_GETHIT")
    return " | ".join(dict.fromkeys(bits)) if bits else "0u"


def sound_pair(
    text: str | None,
    default: tuple[int, int] = (-1, -1),
) -> tuple[int, int]:
    if text is None:
        return default

    value = text.strip()
    if value[:1].lower() in ("s", "f"):
        value = value[1:]
    parts = [p.strip() for p in value.split(",")]
    if len(parts) != 2:
        raise ValueError(f"expected sound group,item, got {text!r}")
    return integer(parts[0]), integer(parts[1])


def simple_trigger(section: Section) -> tuple[str, int, int]:
    triggers = section.all("trigger1")
    if not triggers:
        return "IK_CNS_TRIGGER_ALWAYS", 0, 0
    if len(triggers) != 1:
        raise ValueError(
            f"[{section.name}] unsupported trigger conjunction {triggers!r}"
        )

    expr = triggers[0].strip()

    if expr == "1":
        return "IK_CNS_TRIGGER_ALWAYS", 0, 0

    m = re.fullmatch(r"Time\s*=\s*(-?\d+)", expr, flags=re.I)
    if m:
        return "IK_CNS_TRIGGER_TIME_EQ", int(m.group(1)), 0

    m = re.fullmatch(r"AnimElem\s*=\s*(\d+)", expr, flags=re.I)
    if m:
        return "IK_CNS_TRIGGER_ANIM_ELEM_EQ", int(m.group(1)), 0

    if re.fullmatch(r"AnimTime\s*=\s*0", expr, flags=re.I):
        return "IK_CNS_TRIGGER_ANIM_END", 0, 0

    raise ValueError(f"[{section.name}] unsupported trigger expression {expr!r}")


def hitdef_trigger(
    section: Section,
) -> tuple[str, int, int, int, str, int]:
    triggers = section.all("trigger1")
    trigger2 = section.all("trigger2")
    if not triggers:
        triggers = ["1"]

    base: str | None = None
    base_value = 0
    dist_op = "IK_CNS_P2_DIST_NONE"
    dist_value = 0

    for expr in triggers:
        value = _strip_outer_parens(expr)
        m = re.fullmatch(
            r"p2bodydist\s+X\s*(<|<=|>|>=)\s*(-?\d+)",
            value,
            flags=re.I,
        )
        if m:
            if dist_op != "IK_CNS_P2_DIST_NONE":
                raise ValueError(
                    f"[{section.name}] multiple p2bodydist predicates"
                )
            dist_op = {
                "<": "IK_CNS_P2_DIST_LT",
                "<=": "IK_CNS_P2_DIST_LE",
                ">": "IK_CNS_P2_DIST_GT",
                ">=": "IK_CNS_P2_DIST_GE",
            }[m.group(1)]
            dist_value = int(m.group(2))
            continue

        probe = Section(section.name, [("trigger1", expr)])
        kind, trigger_value, _ = simple_trigger(probe)
        if base is not None:
            raise ValueError(
                f"[{section.name}] unsupported HitDef trigger conjunction"
            )
        base = kind
        base_value = trigger_value

    second_kind = 255
    second_value = 0
    if trigger2:
        if len(trigger2) != 1:
            raise ValueError(
                f"[{section.name}] multiple trigger2 HitDef expressions"
            )
        probe = Section(section.name, [("trigger1", trigger2[0])])
        kind, value, _ = simple_trigger(probe)
        second_kind = kind
        second_value = value

    return (
        base or "IK_CNS_TRIGGER_ALWAYS",
        base_value,
        second_kind,
        second_value,
        dist_op,
        dist_value,
    )


def _strip_outer_parens(text: str) -> str:
    value = text.strip()
    while value.startswith("(") and value.endswith(")"):
        depth = 0
        balanced = True
        for i, ch in enumerate(value):
            if ch == "(":
                depth += 1
            elif ch == ")":
                depth -= 1
                if depth == 0 and i != len(value) - 1:
                    balanced = False
                    break
        if not balanced or depth != 0:
            break
        value = value[1:-1].strip()
    return value


def _anim_elem_range_trigger(expr: str) -> tuple[int, int] | None:
    value = _strip_outer_parens(expr)
    pattern = re.compile(
        r"^\(?\s*AnimElemTime\s*\(\s*(\d+)\s*\)\s*>=\s*0\s*\)?"
        r"\s*&&\s*"
        r"\(?\s*AnimElemTime\s*\(\s*(\d+)\s*\)\s*<\s*0\s*\)?$",
        flags=re.I,
    )
    m = pattern.fullmatch(value)
    if not m:
        return None
    return int(m.group(1)), int(m.group(2))


def _move_contact_window(
    triggers: list[str],
) -> tuple[int, int] | None:
    if len(triggers) != 2:
        return None

    normalized = [_strip_outer_parens(t) for t in triggers]
    if not any(re.fullmatch(r"movecontact", t, flags=re.I) for t in normalized):
        return None

    window = next(
        (
            t
            for t in normalized
            if not re.fullmatch(r"movecontact", t, flags=re.I)
        ),
        None,
    )
    if window is None:
        return None

    pattern = re.compile(
        r"^\(?\s*AnimElemTime\s*\(\s*(\d+)\s*\)\s*>\s*0\s*\)?"
        r"\s*&&\s*"
        r"\(?\s*AnimElemTime\s*\(\s*(\d+)\s*\)\s*<=\s*0\s*\)?$",
        flags=re.I,
    )
    m = pattern.fullmatch(window)
    if not m:
        return None
    return int(m.group(1)), int(m.group(2))


def controller_trigger(
    ctrl: Section,
    ctype: str,
) -> tuple[str, int, int]:
    triggers = ctrl.all("trigger1")
    trigger2 = ctrl.all("trigger2")

    if len(triggers) == 1 and len(trigger2) == 1:
        m0 = re.fullmatch(
            r"AnimElem\s*=\s*(\d+)",
            _strip_outer_parens(triggers[0]),
            flags=re.I,
        )
        m1 = re.fullmatch(
            r"AnimElem\s*=\s*(\d+)",
            _strip_outer_parens(trigger2[0]),
            flags=re.I,
        )
        if m0 and m1:
            return (
                "IK_CNS_TRIGGER_ANIM_ELEM_EQ_OR",
                int(m0.group(1)),
                int(m1.group(1)),
            )

    if ctype in ("width", "targetbind") and len(triggers) == 1:
        parsed = _anim_elem_range_trigger(triggers[0])
        if parsed is not None:
            return "IK_CNS_TRIGGER_ANIM_ELEM_RANGE", parsed[0], parsed[1]

    if ctype == "targetbind" and len(triggers) == 1:
        m = re.fullmatch(
            r"AnimElemTime\s*\(\s*(\d+)\s*\)\s*<\s*0",
            _strip_outer_parens(triggers[0]),
            flags=re.I,
        )
        if m:
            return "IK_CNS_TRIGGER_ANIM_ELEM_BEFORE", int(m.group(1)), 0

    if ctype in ("turn", "posadd", "targetfacing") and len(triggers) == 2:
        normalized = [_strip_outer_parens(t) for t in triggers]
        if any(re.fullmatch(r"var\(\s*2\s*\)", t, flags=re.I)
               for t in normalized):
            elem = next((
                re.fullmatch(r"AnimElem\s*=\s*(\d+)", t, flags=re.I)
                for t in normalized
                if re.fullmatch(r"AnimElem\s*=\s*(\d+)", t, flags=re.I)
            ), None)
            if elem:
                return (
                    "IK_CNS_TRIGGER_STATE_AXIS_FWD_ANIM_ELEM_EQ",
                    int(elem.group(1)), 0,
                )

    if ctype in ("selfstate", "changestate"):
        if ctype == "selfstate" and len(triggers) == 1 and re.fullmatch(
            r"!\s*gethitvar\s*\(\s*isbound\s*\)",
            triggers[0], flags=re.I
        ):
            return "IK_CNS_TRIGGER_NOT_BOUND", 0, 0
        target = integer(ctrl.get("value"), -1)
        trigger_all = ctrl.all("triggerall")
        if target == 5200 and trigger_all:
            return "IK_CNS_TRIGGER_THROW_GROUND_RECOVERY", q8(-20), 0
        if target == 5210 and trigger_all:
            return "IK_CNS_TRIGGER_THROW_AIR_RECOVERY", 0, 0
        normalized = [_strip_outer_parens(t) for t in triggers]
        if ctype == "selfstate":
            split = normalized
            if len(normalized) == 1:
                split = [
                    _strip_outer_parens(p)
                    for p in re.split(r"\s*&&\s*", normalized[0])
                ]
            if (len(split) == 2 and
                any(re.fullmatch(r"Vel\s+Y\s*>\s*0", t, flags=re.I)
                    for t in split) and
                any(re.fullmatch(r"Pos\s+Y\s*>=\s*0", t, flags=re.I)
                    for t in split)):
                return "IK_CNS_TRIGGER_VY_GT_Q8_AT_FLOOR", 0, 0

    if ctype == "width" and len(triggers) == 1:
        parsed = _anim_elem_range_trigger(triggers[0])
        if parsed is not None:
            return "IK_CNS_TRIGGER_ANIM_ELEM_RANGE", parsed[0], parsed[1]

    trigger_all = ctrl.all("triggerall")

    if ctype == "changestate" and len(triggers) in (2, 3):
        normalized = [_strip_outer_parens(t) for t in triggers]
        cmd = next((
            t for t in normalized
            if re.fullmatch(
                r'Command\s*=\s*"a"\s*\|\|\s*Command\s*=\s*"b"',
                t, flags=re.I
            )
        ), None)
        vy = next((
            re.fullmatch(r"Vel\s+y\s*<\s*(-?\d+(?:\.\d+)?)", t, flags=re.I)
            for t in normalized
            if re.fullmatch(r"Vel\s+y\s*<\s*(-?\d+(?:\.\d+)?)", t, flags=re.I)
        ), None)
        time_ok = all(
            re.fullmatch(r"Time\s*>\s*0", t, flags=re.I)
            for t in normalized
            if t != cmd and (not vy or t != vy.group(0))
        )
        if cmd is not None and vy and time_ok:
            return (
                "IK_CNS_TRIGGER_COMMAND_ANY_VY_LT_Q8",
                (1 << 5) | (1 << 6),
                q8(float(vy.group(1))),
            )

    if ctype == "changestate" and len(triggers) == 1 and not trigger_all:
        value = _strip_outer_parens(triggers[0])
        parts = [
            _strip_outer_parens(p)
            for p in re.split(r"\s*&&\s*", value)
        ]
        if len(parts) == 2:
            vy = next((
                re.fullmatch(r"Vel\s+Y\s*>\s*(-?\d+(?:\.\d+)?)", t, flags=re.I)
                for t in parts
                if re.fullmatch(r"Vel\s+Y\s*>\s*(-?\d+(?:\.\d+)?)", t, flags=re.I)
            ), None)
            pos = next((
                re.fullmatch(r"Pos\s+Y\s*>=\s*(-?\d+(?:\.\d+)?)", t, flags=re.I)
                for t in parts
                if re.fullmatch(r"Pos\s+Y\s*>=\s*(-?\d+(?:\.\d+)?)", t, flags=re.I)
            ), None)
            if vy and pos:
                return (
                    "IK_CNS_TRIGGER_VY_GT_Q8_AT_LEVEL",
                    q8(float(pos.group(1))),
                    q8(float(vy.group(1))),
                )

    if ctype == "changestate" and len(trigger_all) == 1 and \
       len(triggers) == 1 and len(trigger2) == 1:
        y = re.fullmatch(
            r"Pos\s+y\s*<\s*(-?\d+)",
            _strip_outer_parens(trigger_all[0]),
            flags=re.I,
        )
        back = re.fullmatch(
            r"BackEdgeBodyDist\s*<=\s*(\d+)",
            _strip_outer_parens(triggers[0]),
            flags=re.I,
        )
        front = re.fullmatch(
            r"FrontEdgeBodyDist\s*<=\s*(\d+)",
            _strip_outer_parens(trigger2[0]),
            flags=re.I,
        )
        if y and back and front and back.group(1) == front.group(1):
            return (
                "IK_CNS_TRIGGER_AIR_NEAR_BODY_EDGE",
                q8(float(y.group(1))),
                int(back.group(1)),
            )

    if ctype == "turn" and len(triggers) == 1:
        value = _strip_outer_parens(triggers[0])
        m = re.fullmatch(
            r"\(?\s*Time\s*=\s*0\s*\)?\s*&&\s*"
            r"\(?\s*FrontEdgeBodyDist\s*<=\s*(\d+)\s*\)?",
            value,
            flags=re.I,
        )
        if m:
            return (
                "IK_CNS_TRIGGER_STATE_ENTRY_FRONT_EDGE_BODY_LE",
                int(m.group(1)), 0,
            )
        m = re.fullmatch(
            r"\(?\s*Time\s*=\s*0\s*\)?\s*&&\s*"
            r"\(?\s*BackEdgeDist\s*<\s*(\d+)\s*\)?",
            value,
            flags=re.I,
        )
        if m:
            return (
                "IK_CNS_TRIGGER_STATE_ENTRY_BACK_EDGE_LT",
                int(m.group(1)), 0,
            )

    if len(triggers) == 1:
        m = re.fullmatch(
            r"Vel\s+Y\s*>=\s*(-?\d+(?:\.\d+)?)",
            _strip_outer_parens(triggers[0]),
            flags=re.I,
        )
        if m:
            return "IK_CNS_TRIGGER_VY_GE_Q8", q8(float(m.group(1))), 0

    if len(triggers) == 1 and re.fullmatch(
        r"HitShakeOver\s*=\s*1",
        _strip_outer_parens(triggers[0]),
        flags=re.I,
    ):
        return "IK_CNS_TRIGGER_HIT_SHAKE_OVER", 0, 0

    if ctype == "changeanim":
        parsed = _move_contact_window(triggers)
        if parsed is not None:
            return (
                "IK_CNS_TRIGGER_MOVE_CONTACT_ELEM_WINDOW",
                parsed[0],
                parsed[1],
            )

    kind, value, value2 = simple_trigger(ctrl)
    if kind == "IK_CNS_TRIGGER_TIME_EQ" and value == 0:
        # Source CNS Time=0 controllers are first evaluated on the runtime's
        # first post-entry state tick, which is numbered 1 internally.
        value = 1
    return kind, value, value2


def constants(globals_: dict[str, Section]) -> dict[str, int]:
    data = globals_.get("data", Section("Data"))
    size = globals_.get("size", Section("Size"))
    vel = globals_.get("velocity", Section("Velocity"))
    movement = globals_.get("movement", Section("Movement"))

    run_fwd = pair(vel.get("run.fwd"))
    run_back = pair(vel.get("run.back"))
    jump_neu = pair(vel.get("jump.neu"))
    run_jump_fwd = pair(vel.get("runjump.fwd"), 4.0, -8.1)
    air_jump_neu = pair(vel.get("airjump.neu"), 0.0, -8.1)
    ground_recover = pair(vel.get("air.gethit.groundrecover"), -.15, -3.5)
    air_recover_mul = pair(vel.get("air.gethit.airrecover.mul"), .5, .2)
    air_recover_add = pair(vel.get("air.gethit.airrecover.add"), 0, -4.5)

    return {
        "life": integer(data.get("life"), 1000),
        "air_juggle": integer(data.get("airjuggle"), 15),
        "ground_back": integer(size.get("ground.back"), 15),
        "ground_front": integer(size.get("ground.front"), 16),
        "air_back": integer(size.get("air.back"), 12),
        "air_front": integer(size.get("air.front"), 12),
        "height": integer(size.get("height"), 60),
        "attack_dist": integer(size.get("attack.dist"), 160),
        "walk_fwd_q8": q8(number(vel.get("walk.fwd"), 2.4)),
        "walk_back_q8": q8(number(vel.get("walk.back"), -2.2)),
        "run_fwd_x_q8": q8(run_fwd[0]),
        "run_fwd_y_q8": q8(run_fwd[1]),
        "run_back_x_q8": q8(run_back[0]),
        "run_back_y_q8": q8(run_back[1]),
        "jump_neu_x_q8": q8(jump_neu[0]),
        "jump_neu_y_q8": q8(jump_neu[1]),
        "jump_back_q8": q8(number(vel.get("jump.back"), -2.55)),
        "jump_fwd_q8": q8(number(vel.get("jump.fwd"), 2.5)),
        "run_jump_fwd_x_q8": q8(run_jump_fwd[0]),
        "run_jump_fwd_y_q8": q8(run_jump_fwd[1]),
        "air_jump_neu_x_q8": q8(air_jump_neu[0]),
        "air_jump_neu_y_q8": q8(air_jump_neu[1]),
        "air_jump_back_q8": q8(number(vel.get("airjump.back"), -2.55)),
        "air_jump_fwd_q8": q8(number(vel.get("airjump.fwd"), 2.5)),
        "air_jump_num": integer(movement.get("airjump.num"), 0),
        "air_jump_height": integer(movement.get("airjump.height"), 35),
        "yaccel_q8": q8(number(movement.get("yaccel"), .44)),
        "stand_friction_q8": q8(number(movement.get("stand.friction"), .85)),
        "crouch_friction_q8": q8(number(movement.get("crouch.friction"), .82)),
        "stand_friction_threshold_q8": q8(
            number(movement.get("stand.friction.threshold"), 2.0)
        ),
        "crouch_friction_threshold_q8": q8(
            number(movement.get("crouch.friction.threshold"), .05)
        ),
        "liedown_time": integer(data.get("liedown.time"), 60),
        "air_gethit_groundlevel_q8": q8(
            number(movement.get("air.gethit.groundlevel"), 25)
        ),
        "air_gethit_trip_groundlevel_q8": q8(
            number(movement.get("air.gethit.trip.groundlevel"), 15)
        ),
        "down_bounce_offset_x_q8": q8(
            pair(movement.get("down.bounce.offset"), 0, 20)[0]
        ),
        "down_bounce_offset_y_q8": q8(
            pair(movement.get("down.bounce.offset"), 0, 20)[1]
        ),
        "down_bounce_yaccel_q8": q8(
            number(movement.get("down.bounce.yaccel"), .4)
        ),
        "down_bounce_groundlevel_q8": q8(
            number(movement.get("down.bounce.groundlevel"), 12)
        ),
        "down_friction_threshold_q8": q8(
            number(movement.get("down.friction.threshold"), .05)
        ),
        "air_gethit_groundrecover_x_q8": q8(ground_recover[0]),
        "air_gethit_groundrecover_y_q8": q8(ground_recover[1]),
        "air_gethit_groundrecover_threshold_q8": q8(
            number(
                movement.get("air.gethit.groundrecover.ground.threshold"),
                -20,
            )
        ),
        "air_gethit_groundrecover_groundlevel_q8": q8(
            number(movement.get("air.gethit.groundrecover.groundlevel"), 10)
        ),
        "air_gethit_airrecover_mul_x_q8": q8(air_recover_mul[0]),
        "air_gethit_airrecover_mul_y_q8": q8(air_recover_mul[1]),
        "air_gethit_airrecover_add_x_q8": q8(air_recover_add[0]),
        "air_gethit_airrecover_add_y_q8": q8(air_recover_add[1]),
        "air_gethit_airrecover_back_q8": q8(
            number(vel.get("air.gethit.airrecover.back"), -1)
        ),
        "air_gethit_airrecover_fwd_q8": q8(
            number(vel.get("air.gethit.airrecover.fwd"), 0)
        ),
        "air_gethit_airrecover_up_q8": q8(
            number(vel.get("air.gethit.airrecover.up"), -2)
        ),
        "air_gethit_airrecover_down_q8": q8(
            number(vel.get("air.gethit.airrecover.down"), 1.5)
        ),
        "air_gethit_airrecover_threshold_q8": q8(
            number(movement.get("air.gethit.airrecover.threshold"), -1)
        ),
        "air_gethit_airrecover_yaccel_q8": q8(
            number(movement.get("air.gethit.airrecover.yaccel"), .35)
        ),
    }


def compile_runtime_controller(
    state_no: int,
    ctrl: Section,
) -> dict | None:
    ctype = (ctrl.get("type", "") or "").strip().lower()

    supported = {
        "changestate",
        "ctrlset",
        "posadd",
        "posset",
        "sprpriority",
        "changeanim",
        "changeanim2",
        "width",
        "varset",
        "targetbind",
        "targetfacing",
        "targetlifeadd",
        "targetstate",
        "turn",
        "selfstate",
        "veladd",
        "velset",
        "velmul",
        "hitvelset",
        "posfreeze",
    }
    if ctype not in supported:
        return None

    trig_kind, trig_value, trig_value2 = controller_trigger(ctrl, ctype)

    flags: list[str] = []
    if integer(ctrl.get("ignorehitpause"), 0):
        flags.append("IK_CNS_CTRL_IGNORE_HIT_PAUSE")

    def flag_expr() -> str:
        return " | ".join(flags) if flags else "0u"

    if ctype == "changestate":
        has_ctrl = ctrl.get("ctrl") is not None
        if has_ctrl:
            flags.append("IK_CNS_CTRL_HAS_CTRL")
        return {
            "state_number": state_no,
            "type": "IK_CNS_CTRL_CHANGE_STATE",
            "trigger_kind": trig_kind,
            "trigger_value": trig_value,
            "trigger_value2": trig_value2,
            "value0": integer(ctrl.get("value")),
            "value1": integer(ctrl.get("ctrl"), 0),
            "flags": flag_expr(),
        }

    if ctype == "ctrlset":
        return {
            "state_number": state_no,
            "type": "IK_CNS_CTRL_CTRL_SET",
            "trigger_kind": trig_kind,
            "trigger_value": trig_value,
            "trigger_value2": trig_value2,
            "value0": integer(ctrl.get("value")),
            "value1": 0,
            "flags": flag_expr(),
        }

    if ctype == "posadd":
        x_text = (ctrl.get("x") or "0").strip()
        m = re.fullmatch(
            r"(-?\d+)\s*-\s*BackEdgeBodyDist",
            x_text,
            flags=re.I,
        )
        if m and ctrl.get("y") is None:
            return {
                "state_number": state_no,
                "type": "IK_CNS_CTRL_POS_ADD_FROM_BACK_EDGE",
                "trigger_kind": trig_kind,
                "trigger_value": trig_value,
                "trigger_value2": trig_value2,
                "value0": q8(float(m.group(1))),
                "value1": 0,
                "flags": flag_expr(),
            }
        x = number(x_text, 0)
        y = number(ctrl.get("y"), 0)
        return {
            "state_number": state_no,
            "type": "IK_CNS_CTRL_POS_ADD",
            "trigger_kind": trig_kind,
            "trigger_value": trig_value,
            "trigger_value2": trig_value2,
            "value0": q8(x),
            "value1": q8(y),
            "flags": flag_expr(),
        }

    if ctype == "posset":
        x = number(ctrl.get("x"), 0)
        y = number(ctrl.get("y"), 0)
        axis: list[str] = []
        if ctrl.get("x") is not None:
            axis.append("IK_CNS_CTRL_AXIS_X")
        if ctrl.get("y") is not None:
            axis.append("IK_CNS_CTRL_AXIS_Y")
        return {
            "state_number": state_no,
            "type": "IK_CNS_CTRL_POS_SET",
            "trigger_kind": trig_kind,
            "trigger_value": trig_value,
            "trigger_value2": trig_value2,
            "value0": q8(x),
            "value1": q8(y),
            "flags": " | ".join(axis + flags) if axis or flags else "0u",
        }

    if ctype == "sprpriority":
        return {
            "state_number": state_no,
            "type": "IK_CNS_CTRL_SPR_PRIORITY",
            "trigger_kind": trig_kind,
            "trigger_value": trig_value,
            "trigger_value2": trig_value2,
            "value0": integer(ctrl.get("value")),
            "value1": 0,
            "flags": flag_expr(),
        }

    if ctype == "changeanim":
        return {
            "state_number": state_no,
            "type": "IK_CNS_CTRL_CHANGE_ANIM",
            "trigger_kind": trig_kind,
            "trigger_value": trig_value,
            "trigger_value2": trig_value2,
            "value0": integer(ctrl.get("value")),
            "value1": integer(ctrl.get("elem"), 1),
            "flags": flag_expr(),
        }

    if ctype == "changeanim2":
        return {
            "state_number": state_no,
            "type": "IK_CNS_CTRL_CHANGE_ANIM2",
            "trigger_kind": trig_kind,
            "trigger_value": trig_value,
            "trigger_value2": trig_value2,
            "value0": integer(ctrl.get("value")),
            "value1": 1,
            "flags": flag_expr(),
        }

    if ctype == "varset":
        var2 = ctrl.get("var(2)")
        if var2 is None or not re.fullmatch(
            r'command\s*=\s*"holdfwd"', var2.strip(), flags=re.I
        ):
            return None
        return {
            "state_number": state_no,
            "type": "IK_CNS_CTRL_CAPTURE_COMMAND_AXIS",
            "trigger_kind": trig_kind,
            "trigger_value": trig_value,
            "trigger_value2": trig_value2,
            "value0": 0,
            "value1": 0,
            "flags": flag_expr(),
        }

    if ctype == "targetbind":
        x, y = pair(ctrl.get("pos"), 0, 0)
        return {
            "state_number": state_no,
            "type": "IK_CNS_CTRL_TARGET_BIND",
            "trigger_kind": trig_kind,
            "trigger_value": trig_value,
            "trigger_value2": trig_value2,
            "value0": q8(x),
            "value1": q8(y),
            "flags": flag_expr(),
        }

    if ctype == "targetfacing":
        return {
            "state_number": state_no,
            "type": "IK_CNS_CTRL_TARGET_FACING",
            "trigger_kind": trig_kind,
            "trigger_value": trig_value,
            "trigger_value2": trig_value2,
            "value0": integer(ctrl.get("value"), 1),
            "value1": 0,
            "flags": flag_expr(),
        }

    if ctype == "targetlifeadd":
        return {
            "state_number": state_no,
            "type": "IK_CNS_CTRL_TARGET_LIFE_ADD",
            "trigger_kind": trig_kind,
            "trigger_value": trig_value,
            "trigger_value2": trig_value2,
            "value0": integer(ctrl.get("value")),
            "value1": 0,
            "flags": flag_expr(),
        }

    if ctype == "targetstate":
        return {
            "state_number": state_no,
            "type": "IK_CNS_CTRL_TARGET_STATE",
            "trigger_kind": trig_kind,
            "trigger_value": trig_value,
            "trigger_value2": trig_value2,
            "value0": integer(ctrl.get("value")),
            "value1": 0,
            "flags": flag_expr(),
        }

    if ctype == "turn":
        return {
            "state_number": state_no,
            "type": "IK_CNS_CTRL_TURN",
            "trigger_kind": trig_kind,
            "trigger_value": trig_value,
            "trigger_value2": trig_value2,
            "value0": 0,
            "value1": 0,
            "flags": flag_expr(),
        }

    if ctype == "posfreeze":
        flags2: list[str] = []
        if integer(ctrl.get("x"), 0):
            flags2.append("IK_CNS_CTRL_AXIS_X")
        if integer(ctrl.get("y"), 0):
            flags2.append("IK_CNS_CTRL_AXIS_Y")
        return {
            "state_number": state_no,
            "type": "IK_CNS_CTRL_POS_FREEZE",
            "trigger_kind": trig_kind,
            "trigger_value": trig_value,
            "trigger_value2": trig_value2,
            "value0": 0,
            "value1": 0,
            "flags": " | ".join(flags2 + flags) if flags2 or flags else "0u",
        }

    if ctype == "selfstate":
        return {
            "state_number": state_no,
            "type": "IK_CNS_CTRL_SELF_STATE",
            "trigger_kind": trig_kind,
            "trigger_value": trig_value,
            "trigger_value2": trig_value2,
            "value0": integer(ctrl.get("value")),
            "value1": 0,
            "flags": flag_expr(),
        }

    if ctype == "veladd":
        x = number(ctrl.get("x"), 0)
        y = number(ctrl.get("y"), 0)
        axis: list[str] = []
        if ctrl.get("x") is not None:
            axis.append("IK_CNS_CTRL_AXIS_X")
        if ctrl.get("y") is not None:
            axis.append("IK_CNS_CTRL_AXIS_Y")
        return {
            "state_number": state_no,
            "type": "IK_CNS_CTRL_VEL_ADD",
            "trigger_kind": trig_kind,
            "trigger_value": trig_value,
            "trigger_value2": trig_value2,
            "value0": q8(x),
            "value1": q8(y),
            "flags": " | ".join(axis + flags) if axis or flags else "0u",
        }

    if ctype == "velset":
        x = number(ctrl.get("x"), 0)
        y = number(ctrl.get("y"), 0)
        axis: list[str] = []
        if ctrl.get("x") is not None:
            axis += ["IK_CNS_CTRL_AXIS_X", "IK_CNS_CTRL_LOCAL_X"]
        if ctrl.get("y") is not None:
            axis.append("IK_CNS_CTRL_AXIS_Y")
        return {
            "state_number": state_no,
            "type": "IK_CNS_CTRL_VEL_SET",
            "trigger_kind": trig_kind,
            "trigger_value": trig_value,
            "trigger_value2": trig_value2,
            "value0": q8(x),
            "value1": q8(y),
            "flags": " | ".join(axis + flags) if axis or flags else "0u",
        }

    if ctype == "hitvelset":
        axis: list[str] = []
        if integer(ctrl.get("x"), 0):
            axis.append("IK_CNS_CTRL_AXIS_X")
        if integer(ctrl.get("y"), 0):
            axis.append("IK_CNS_CTRL_AXIS_Y")
        return {
            "state_number": state_no,
            "type": "IK_CNS_CTRL_HIT_VEL_SET",
            "trigger_kind": trig_kind,
            "trigger_value": trig_value,
            "trigger_value2": trig_value2,
            "value0": 0,
            "value1": 0,
            "flags": " | ".join(axis + flags) if axis or flags else "0u",
        }

    if ctype == "velmul":
        xexpr = (ctrl.get("x") or "").strip()
        m = re.fullmatch(
            r"([+-]?(?:\d+(?:\.\d*)?|\.\d+))\s*\*\s*"
            r"ifelse\s*\(\s*AnimElemTime\s*\(\s*(\d+)\s*\)"
            r"\s*<\s*0\s*,\s*1\s*,\s*"
            r"([+-]?(?:\d+(?:\.\d*)?|\.\d+))\s*\)",
            xexpr,
            flags=re.I,
        )
        if not m or ctrl.get("y") is not None:
            return None
        base = float(m.group(1))
        elem = int(m.group(2))
        tail = float(m.group(3))
        return {
            "state_number": state_no,
            "type": "IK_CNS_CTRL_VEL_MUL_X_BY_ANIM_ELEM",
            "trigger_kind": trig_kind,
            "trigger_value": elem,
            "trigger_value2": 0,
            "value0": q8(base),
            "value1": q8(base * tail),
            "flags": flag_expr(),
        }

    width_front, width_back = pair(
        ctrl.get("value", ctrl.get("edge")), 0, 0
    )
    if int(width_front) != width_front or int(width_back) != width_back:
        raise ValueError(
            f"[{ctrl.name}] Width value must be integral in the Saturn subset"
        )
    return {
        "state_number": state_no,
        "type": "IK_CNS_CTRL_WIDTH",
        "trigger_kind": trig_kind,
        "trigger_value": trig_value,
        "trigger_value2": trig_value2,
        "value0": int(width_front),
        "value1": int(width_back),
        "flags": flag_expr(),
    }


def parse_state(
    state: State,
    hitdef_ofs: int,
    sound_ofs: int,
    controller_ofs: int,
):
    sd = state.statedef

    state_type = STATE_TYPE.get(
        (sd.get("type", "U") or "U").strip().upper()
    )
    move_type = MOVE_TYPE.get(
        (sd.get("movetype", "U") or "U").strip().upper()
    )
    physics = PHYSICS.get(
        (sd.get("physics", "N") or "N").strip().upper()
    )
    if state_type is None or move_type is None or physics is None:
        raise ValueError(f"state {state.number}: unsupported Statedef type")

    vx, vy = pair(sd.get("velset"))
    has_velset = sd.get("velset") is not None

    hitdefs: list[dict] = []
    sounds: list[dict] = []
    controllers: list[dict] = []
    unsupported: list[str] = []

    for ctrl in state.controllers:
        ctype = (ctrl.get("type", "") or "").strip().lower()

        if ctype == "hitdef":
            (
                trig_kind,
                trig_value,
                trig2_kind,
                trig2_value,
                p2_dist_op,
                p2_dist_x,
            ) = hitdef_trigger(ctrl)

            damage_text = ctrl.get("damage") or "0,0"
            alt_damage = -1
            alt_damage_prev_state = -32768
            damage_expr = re.fullmatch(
                r"\s*(-?\d+)\s*\+\s*"
                r"\(\s*prevstateno\s*=\s*(-?\d+)\s*\)"
                r"\s*\*\s*(-?\d+)\s*,\s*(-?\d+)\s*",
                damage_text,
                flags=re.I,
            )
            if damage_expr:
                damage = int(damage_expr.group(1))
                alt_damage_prev_state = int(damage_expr.group(2))
                alt_damage = damage + int(damage_expr.group(3))
                guard_damage = int(damage_expr.group(4))
            else:
                damage, guard_damage = pair(damage_text, 0, 0)
            pause1, pause2 = pair(ctrl.get("pausetime"), 0, 0)
            gx, gy = pair(ctrl.get("ground.velocity"), 0, 0)
            ax, ay = pair(ctrl.get("air.velocity"), gx, gy)
            guardx = number(ctrl.get("guard.velocity"), gx)
            agx, agy = pair(
                ctrl.get("airguard.velocity"),
                ax * 1.5,
                ay / 2.0,
            )
            fall_x_text = ctrl.get("fall.xvelocity")
            fall_x = number(fall_x_text, 0)
            fall_y = number(ctrl.get("fall.yvelocity"), -4.5)
            down_x, down_y = pair(ctrl.get("down.velocity"), ax, ay)
            down_hit_time = integer(ctrl.get("down.hittime"), 0)
            sparkx, sparky = pair(ctrl.get("sparkxy"), 0, 0)
            hs = sound_pair(ctrl.get("hitsound"))
            gs = sound_pair(ctrl.get("guardsound"))

            flags: list[str] = []
            if integer(ctrl.get("fall"), 0):
                flags.append("IK_CNS_HITDEF_FALL")
            if integer(ctrl.get("air.fall"), 0):
                flags.append("IK_CNS_HITDEF_AIR_FALL")
            if integer(ctrl.get("forcestand"), 0):
                flags.append("IK_CNS_HITDEF_FORCE_STAND")
            if integer(ctrl.get("forcenofall"), 0):
                flags.append("IK_CNS_HITDEF_FORCE_NO_FALL")
            attr_parts = [p.strip().upper() for p in (ctrl.get("attr") or "").split(",")]
            if len(attr_parts) > 1 and attr_parts[1].endswith("T"):
                flags.append("IK_CNS_HITDEF_THROW")

            hitdefs.append(
                {
                    "state_number": state.number,
                    "trigger_kind": trig_kind,
                    "trigger_value": trig_value,
                    "trigger2_kind": trig2_kind,
                    "trigger2_value": trig2_value,
                    "damage": int(damage),
                    "guard_damage": int(guard_damage),
                    "priority": integer(
                        (ctrl.get("priority") or "4").split(",", 1)[0],
                        4,
                    ),
                    "priority_type": priority_type_code(ctrl.get("priority")),
                    "pause_p1": int(pause1),
                    "pause_p2": int(pause2),
                    "ground_type": GROUND_TYPE.get(
                        (
                            ctrl.get("ground.type", "normal") or "normal"
                        ).strip().lower(),
                        "IK_CNS_GROUND_NORMAL",
                    ),
                    "ground_slide_time": integer(
                        ctrl.get("ground.slidetime"), 0
                    ),
                    "ground_hit_time": integer(
                        ctrl.get("ground.hittime"), 0
                    ),
                    "air_hit_time": integer(
                        ctrl.get("air.hittime"),
                        integer(ctrl.get("ground.hittime"), 0),
                    ),
                    "ground_velocity_x_q8": q8(gx),
                    "ground_velocity_y_q8": q8(gy),
                    "air_velocity_x_q8": q8(ax),
                    "air_velocity_y_q8": q8(ay),
                    "spark_no": integer(ctrl.get("sparkno"), -1),
                    "spark_x": int(sparkx),
                    "spark_y": int(sparky),
                    "hit_sound_group": hs[0],
                    "hit_sound_item": hs[1],
                    "guard_sound_group": gs[0],
                    "guard_sound_item": gs[1],
                    "flags": " | ".join(flags) if flags else "0u",
                    "guard_flags": guard_mask(ctrl.get("guardflag")),
                    "guard_kill": integer(ctrl.get("guard.kill"), 1),
                    "hit_flags": hit_mask(ctrl.get("hitflag")),
                    "guard_slide_time": integer(
                        ctrl.get("guard.slidetime"),
                        integer(ctrl.get("ground.slidetime"), 0),
                    ),
                    "guard_hit_time": integer(
                        ctrl.get("guard.hittime"),
                        integer(ctrl.get("ground.hittime"), 0),
                    ),
                    "guard_ctrl_time": integer(
                        ctrl.get("guard.ctrltime"),
                        integer(
                            ctrl.get("guard.hittime"),
                            integer(ctrl.get("ground.hittime"), 0),
                        ),
                    ),
                    "guard_velocity_x_q8": q8(guardx),
                    "air_guard_velocity_x_q8": q8(agx),
                    "air_guard_velocity_y_q8": q8(agy),
                    "anim_type": anim_type_code(ctrl.get("animtype")),
                    "air_anim_type": anim_type_code(
                        ctrl.get("air.animtype", ctrl.get("animtype", "light"))
                    ),
                    "fall_x_velocity_q8": q8(fall_x),
                    "fall_y_velocity_q8": q8(fall_y),
                    "fall_x_velocity_set": int(fall_x_text is not None),
                    "fall_recover": integer(ctrl.get("fall.recover"), 1),
                    "fall_recover_time": integer(
                        ctrl.get("fall.recovertime"), 4
                    ),
                    "down_hit_time": down_hit_time,
                    "down_velocity_x_q8": q8(down_x),
                    "down_velocity_y_q8": q8(down_y),
                    "down_bounce": integer(ctrl.get("down.bounce"), 0),
                    "air_juggle": integer(ctrl.get("air.juggle"), 0),
                    "p1_state_no": integer(ctrl.get("p1stateno"), -1),
                    "p2_state_no": integer(ctrl.get("p2stateno"), -1),
                    "guard_dist": integer(ctrl.get("guard.dist"), -1),
                    "p1_facing": integer(ctrl.get("p1facing"), 0),
                    "p2_facing": integer(ctrl.get("p2facing"), 0),
                    "p1_spr_priority": integer(ctrl.get("p1sprpriority"), -128),
                    "p2_body_dist_op": p2_dist_op,
                    "p2_body_dist_x": p2_dist_x,
                    "alt_damage": alt_damage,
                    "alt_damage_prev_state": alt_damage_prev_state,
                    "yaccel_q8": q8(number(ctrl.get("yaccel"), 0)),
                }
            )

        elif ctype == "playsnd":
            trig_kind, trig_value, _ = simple_trigger(ctrl)
            group, item = sound_pair(ctrl.get("value"))
            sounds.append(
                {
                    "state_number": state.number,
                    "trigger_kind": trig_kind,
                    "trigger_value": trig_value,
                    "group": group,
                    "item": item,
                }
            )

        else:
            try:
                compiled = compile_runtime_controller(state.number, ctrl)
            except ValueError:
                compiled = None

            if compiled is not None:
                controllers.append(compiled)
            elif ctype:
                unsupported.append(ctype)

    if len(hitdefs) > 32:
        raise ValueError(
            f"state {state.number}: more than 32 HitDefs are not supported"
        )

    state_row = {
        "number": state.number,
        "anim": integer(sd.get("anim"), -1),
        "power_add": integer(sd.get("poweradd"), 0),
        "velset_x_q8": q8(vx),
        "velset_y_q8": q8(vy),
        "state_type": state_type,
        "move_type": move_type,
        "physics": physics,
        "ctrl": integer(sd.get("ctrl"), 0),
        "spr_priority": integer(sd.get("sprpriority"), 0),
        "has_velset": int(has_velset),
        "hitdef_ofs": hitdef_ofs,
        "hitdef_count": len(hitdefs),
        "playsnd_ofs": sound_ofs,
        "playsnd_count": len(sounds),
        "controller_ofs": controller_ofs,
        "controller_count": len(controllers),
        "juggle": integer(sd.get("juggle"), 0),
        "has_juggle": int(sd.get("juggle") is not None),
        "owns_air_accel": int(any(
            c["type"] == "IK_CNS_CTRL_VEL_ADD" and
            "IK_CNS_CTRL_AXIS_Y" in c["flags"]
            for c in controllers
        )),
        "hitdef_persist": integer(sd.get("hitdefpersist"), 0),
        "unsupported_controllers": sorted(set(unsupported)),
    }

    return state_row, hitdefs, sounds, controllers



@dataclass
class ZssState:
    number: int
    metadata: dict[str, str]
    body: str


def parse_zss_states(path: Path) -> dict[int, ZssState]:
    text = path.read_text(encoding="utf-8-sig", errors="strict")
    matches = list(re.finditer(
        r"\[StateDef\s+(-?\d+)\s*;([^\]]*)\]",
        text,
        flags=re.I,
    ))
    states: dict[int, ZssState] = {}
    for i, match in enumerate(matches):
        number_ = int(match.group(1))
        meta: dict[str, str] = {}
        for part in match.group(2).split(";"):
            part = part.strip()
            if not part or ":" not in part:
                continue
            key, value = part.split(":", 1)
            meta[key.strip().lower()] = value.strip()
        end = matches[i + 1].start() if i + 1 < len(matches) else len(text)
        states[number_] = ZssState(number_, meta, text[match.end():end])
    return states


def _common_ctrl(
    state_no: int,
    ctype: str,
    trigger_kind: str = "IK_CNS_TRIGGER_ALWAYS",
    trigger_value: int | str = 0,
    trigger_value2: int | str = 0,
    value0: int | str = 0,
    value1: int | str = 0,
    flags: str = "0u",
) -> dict:
    return {
        "state_number": state_no,
        "type": ctype,
        "trigger_kind": trigger_kind,
        "trigger_value": trigger_value,
        "trigger_value2": trigger_value2,
        "value0": value0,
        "value1": value1,
        "flags": flags,
    }


def compile_common_states(
    path: Path,
    selected: list[int],
    const: dict[str, int],
    controller_ofs: int,
) -> tuple[list[dict], list[dict], dict[int, list[str]]]:
    """Lower the first common1.zss locomotion subset to generic CNS ops.

    This is deliberately strict: only states whose ZSS semantics have an
    explicit lowering below are accepted. Source-state presence is verified,
    and deferred presentation-only behavior is surfaced in the JSON report.
    """
    source = parse_zss_states(path)
    missing = [n for n in selected if n not in source]
    if missing:
        raise ValueError(f"missing common Statedef(s): {missing}")

    supported = {
        0, 10, 11, 12, 20, 40, 45, 50, 51, 52, 100, 105, 106,
        120, 130, 131, 132, 140, 150, 151, 152, 153, 154, 155,
        5000, 5001, 5010, 5011, 5020, 5030, 5035, 5040, 5050,
        5070, 5071, 5080, 5081, 5100, 5101, 5110, 5120, 5150,
        5200, 5201, 5210,
    }
    unsupported = sorted(set(selected) - supported)
    if unsupported:
        raise ValueError(
            "common1 states do not have a Saturn lowering yet: "
            f"{unsupported}"
        )

    def meta(n: int, key: str, default: str) -> str:
        return source[n].metadata.get(key, default).strip().upper()

    def state_row(
        n: int,
        anim: int,
        ctrl: int,
        land_state: int = 0,
        spr: int | None = None,
    ) -> dict:
        stype = STATE_TYPE.get(meta(n, "type", "U"))
        physics = PHYSICS.get(meta(n, "physics", "N"))
        if stype is None or physics is None:
            raise ValueError(f"common state {n}: unsupported StateDef metadata")
        priority = (
            integer(source[n].metadata.get("sprpriority"), 0)
            if spr is None else spr
        )
        move_type = MOVE_TYPE.get(meta(n, "movetype", "I"))
        if move_type is None:
            raise ValueError(f"common state {n}: unsupported move type")
        return {
            "number": n,
            "anim": anim,
            "power_add": 0,
            "velset_x_q8": 0,
            "velset_y_q8": 0,
            "state_type": stype,
            "move_type": move_type,
            "physics": physics,
            "ctrl": ctrl,
            "spr_priority": priority,
            "has_velset": 0,
            "hitdef_ofs": 0,
            "hitdef_count": 0,
            "playsnd_ofs": 0,
            "playsnd_count": 0,
            "controller_ofs": 0,
            "controller_count": 0,
            "land_state": land_state,
            "air_accel_q8": 0,
            "land_level_q8": 0,
            "air_motion_start": 0,
            "land_ctrl": 0,
            "juggle": 0,
            "has_juggle": 0,
            "owns_air_accel": 0,
            "unsupported_controllers": [],
        }

    rows: list[dict] = []
    controllers: list[dict] = []
    deferred: dict[int, list[str]] = {}

    for n in selected:
        begin = controller_ofs + len(controllers)
        cs: list[dict] = []
        row: dict

        if n == 0:
            row = state_row(0, 0, 1)
            cs += [
                _common_ctrl(
                    0, "IK_CNS_CTRL_VEL_SET",
                    "IK_CNS_TRIGGER_TIME_EQ", 4, 0, 0, 0,
                    "IK_CNS_CTRL_AXIS_X",
                ),
                _common_ctrl(
                    0, "IK_CNS_CTRL_VEL_SET",
                    "IK_CNS_TRIGGER_ABS_VX_LT_Q8",
                    const["stand_friction_threshold_q8"], 0, 0, 0,
                    "IK_CNS_CTRL_AXIS_X",
                ),
                _common_ctrl(
                    0, "IK_CNS_CTRL_CHANGE_STATE",
                    "IK_CNS_TRIGGER_NOT_ALIVE", 0, 0, 5050, 0,
                ),
            ]

        elif n == 10:
            row = state_row(10, 10, 0)
            cs += [
                _common_ctrl(
                    10, "IK_CNS_CTRL_VEL_MUL",
                    "IK_CNS_TRIGGER_TIME_EQ", 1, 0, q8(.75), 0,
                    "IK_CNS_CTRL_AXIS_X",
                ),
                _common_ctrl(
                    10, "IK_CNS_CTRL_VEL_SET",
                    "IK_CNS_TRIGGER_ABS_VX_LT_Q8",
                    const["crouch_friction_threshold_q8"], 0, 0, 0,
                    "IK_CNS_CTRL_AXIS_X",
                ),
                _common_ctrl(
                    10, "IK_CNS_CTRL_CHANGE_STATE",
                    "IK_CNS_TRIGGER_ANIM_END", 0, 0, 11, 0,
                ),
            ]

        elif n == 11:
            row = state_row(11, 11, 1)
            cs += [
                _common_ctrl(
                    11, "IK_CNS_CTRL_CHANGE_ANIM_IF_END_FROM",
                    "IK_CNS_TRIGGER_ALWAYS", 0, 0, 6, 11,
                ),
                _common_ctrl(
                    11, "IK_CNS_CTRL_VEL_SET",
                    "IK_CNS_TRIGGER_ABS_VX_LT_Q8",
                    const["crouch_friction_threshold_q8"], 0, 0, 0,
                    "IK_CNS_CTRL_AXIS_X",
                ),
            ]

        elif n == 12:
            row = state_row(12, 12, 0)
            cs.append(_common_ctrl(
                12, "IK_CNS_CTRL_CHANGE_STATE",
                "IK_CNS_TRIGGER_ANIM_END", 0, 0, 0, 0,
            ))

        elif n == 20:
            row = state_row(20, -1, 1)
            cs += [
                _common_ctrl(
                    20, "IK_CNS_CTRL_VEL_SET",
                    "IK_CNS_TRIGGER_COMMAND_ACTIVE",
                    "IK_CNS_COMMAND_HOLD_BACK", 0,
                    const["walk_back_q8"], 0,
                    "IK_CNS_CTRL_AXIS_X | IK_CNS_CTRL_LOCAL_X",
                ),
                _common_ctrl(
                    20, "IK_CNS_CTRL_VEL_SET",
                    "IK_CNS_TRIGGER_COMMAND_ACTIVE",
                    "IK_CNS_COMMAND_HOLD_FWD", 0,
                    const["walk_fwd_q8"], 0,
                    "IK_CNS_CTRL_AXIS_X | IK_CNS_CTRL_LOCAL_X",
                ),
                _common_ctrl(
                    20, "IK_CNS_CTRL_CHANGE_ANIM_BY_VX",
                    "IK_CNS_TRIGGER_ALWAYS", 0, 0, -1, 20,
                ),
            ]

        elif n == 40:
            row = state_row(40, 40, 0, spr=1)
            cs += [
                _common_ctrl(
                    40, "IK_CNS_CTRL_CAPTURE_COMMAND_AXIS",
                    "IK_CNS_TRIGGER_ALWAYS",
                ),
                _common_ctrl(
                    40, "IK_CNS_CTRL_JUMP_LAUNCH",
                    "IK_CNS_TRIGGER_ANIM_END",
                ),
                _common_ctrl(
                    40, "IK_CNS_CTRL_CHANGE_STATE",
                    "IK_CNS_TRIGGER_ANIM_END", 0, 0, 50, 1,
                    "IK_CNS_CTRL_HAS_CTRL",
                ),
            ]

        elif n == 45:
            row = state_row(45, 41, 0)
            row["has_velset"] = 1
            cs += [
                _common_ctrl(
                    45, "IK_CNS_CTRL_CHANGE_ANIM_IF_EXISTS",
                    "IK_CNS_TRIGGER_TIME_EQ", 1, 0, 44, 41,
                ),
                _common_ctrl(
                    45, "IK_CNS_CTRL_CAPTURE_COMMAND_AXIS",
                    "IK_CNS_TRIGGER_ALWAYS",
                ),
                _common_ctrl(
                    45, "IK_CNS_CTRL_AIR_JUMP_LAUNCH",
                    "IK_CNS_TRIGGER_TIME_EQ", 2,
                ),
                _common_ctrl(
                    45, "IK_CNS_CTRL_CHANGE_STATE",
                    "IK_CNS_TRIGGER_TIME_EQ", 2, 0, 50, 1,
                    "IK_CNS_CTRL_HAS_CTRL",
                ),
            ]

        elif n == 50:
            row = state_row(50, -1, 0, land_state=52)
            cs += [
                _common_ctrl(
                    50, "IK_CNS_CTRL_CHANGE_ANIM_BY_VX",
                    "IK_CNS_TRIGGER_TIME_EQ", 1, 0, 41, 42,
                ),
                _common_ctrl(
                    50, "IK_CNS_CTRL_CHANGE_ANIM_DESCENT_IF_EXISTS",
                    "IK_CNS_TRIGGER_ALWAYS", 0, 0, q8(-2), 41,
                ),
            ]

        elif n == 51:
            row = state_row(51, -1, 0, land_state=52)

        elif n == 52:
            row = state_row(52, 47, 0)
            cs += [
                _common_ctrl(
                    52, "IK_CNS_CTRL_VEL_SET",
                    "IK_CNS_TRIGGER_TIME_EQ", 1, 0, 0, 0,
                    "IK_CNS_CTRL_AXIS_Y",
                ),
                _common_ctrl(
                    52, "IK_CNS_CTRL_POS_SET",
                    "IK_CNS_TRIGGER_TIME_EQ", 1, 0, 0, 0,
                    "IK_CNS_CTRL_AXIS_Y",
                ),
                _common_ctrl(
                    52, "IK_CNS_CTRL_CTRL_SET",
                    "IK_CNS_TRIGGER_TIME_EQ", 3, 0, 1, 0,
                ),
                _common_ctrl(
                    52, "IK_CNS_CTRL_VEL_SET",
                    "IK_CNS_TRIGGER_ABS_VX_LT_Q8",
                    const["stand_friction_threshold_q8"], 0, 0, 0,
                    "IK_CNS_CTRL_AXIS_X",
                ),
                _common_ctrl(
                    52, "IK_CNS_CTRL_CHANGE_STATE",
                    "IK_CNS_TRIGGER_ANIM_END", 0, 0, 0, 1,
                    "IK_CNS_CTRL_HAS_CTRL",
                ),
            ]

        elif n == 100:
            row = state_row(100, 100, 1)
            cs += [
                _common_ctrl(
                    100, "IK_CNS_CTRL_VEL_SET",
                    "IK_CNS_TRIGGER_ALWAYS", 0, 0,
                    const["run_fwd_x_q8"], 0,
                    "IK_CNS_CTRL_AXIS_X | IK_CNS_CTRL_LOCAL_X",
                ),
                _common_ctrl(
                    100, "IK_CNS_CTRL_CHANGE_STATE",
                    "IK_CNS_TRIGGER_COMMAND_INACTIVE",
                    "IK_CNS_COMMAND_HOLD_FWD", 0, 0, 0,
                ),
            ]
            deferred[n] = ["AssertSpecial noWalk/noAutoTurn"]

        elif n == 105:
            row = state_row(105, 105, 0, land_state=106)
            cs += [
                _common_ctrl(
                    105, "IK_CNS_CTRL_VEL_SET",
                    "IK_CNS_TRIGGER_TIME_EQ", 1, 0,
                    const["run_back_x_q8"], const["run_back_y_q8"],
                    "IK_CNS_CTRL_AXIS_X | IK_CNS_CTRL_AXIS_Y | "
                    "IK_CNS_CTRL_LOCAL_X",
                ),
                _common_ctrl(
                    105, "IK_CNS_CTRL_CTRL_SET",
                    "IK_CNS_TRIGGER_TIME_EQ", 2, 0, 1, 0,
                ),
            ]

        elif n == 120:
            row = state_row(120, -1, 0)
            cs += [
                _common_ctrl(
                    120, "IK_CNS_CTRL_GUARD_ANIM_BY_TYPE",
                    "IK_CNS_TRIGGER_TIME_EQ", 1, 0, 120, 0,
                ),
                _common_ctrl(
                    120, "IK_CNS_CTRL_GUARD_STATE_BY_TYPE",
                    "IK_CNS_TRIGGER_ANIM_END", 0, 0, 130, 0,
                ),
            ]

        elif n == 130:
            row = state_row(130, 130, 0)
            cs.append(_common_ctrl(
                130, "IK_CNS_CTRL_CHANGE_STATE",
                "IK_CNS_TRIGGER_COMMAND_ACTIVE",
                "IK_CNS_COMMAND_HOLD_DOWN", 0, 131, 0,
            ))

        elif n == 131:
            row = state_row(131, 131, 0)
            cs.append(_common_ctrl(
                131, "IK_CNS_CTRL_CHANGE_STATE",
                "IK_CNS_TRIGGER_COMMAND_INACTIVE",
                "IK_CNS_COMMAND_HOLD_DOWN", 0, 130, 0,
            ))

        elif n == 132:
            row = state_row(132, 132, 0, land_state=130)

        elif n == 140:
            row = state_row(140, -1, 1)
            cs += [
                _common_ctrl(
                    140, "IK_CNS_CTRL_GUARD_ANIM_BY_TYPE",
                    "IK_CNS_TRIGGER_TIME_EQ", 1, 0, 140, 0,
                ),
                _common_ctrl(
                    140, "IK_CNS_CTRL_GUARD_END",
                    "IK_CNS_TRIGGER_ANIM_END",
                ),
            ]

        elif n == 150:
            row = state_row(150, 150, 0)
            row["has_velset"] = 1
            cs.append(_common_ctrl(
                150, "IK_CNS_CTRL_CHANGE_STATE",
                "IK_CNS_TRIGGER_TIME_EQ", 1, 0, 151, 0,
            ))
            deferred[n] = ["ForceFeedback"]

        elif n == 151:
            row = state_row(151, 150, 0)
            cs += [
                _common_ctrl(
                    151, "IK_CNS_CTRL_HIT_VEL_SET",
                    "IK_CNS_TRIGGER_TIME_EQ", 1, 0, 0, 0,
                    "IK_CNS_CTRL_AXIS_X",
                ),
                _common_ctrl(
                    151, "IK_CNS_CTRL_VEL_SET",
                    "IK_CNS_TRIGGER_HIT_SLIDE_TIME", 0, 0, 0, 0,
                    "IK_CNS_CTRL_AXIS_X",
                ),
                _common_ctrl(
                    151, "IK_CNS_CTRL_CTRL_SET",
                    "IK_CNS_TRIGGER_HIT_CTRL_TIME", 0, 0, 1, 0,
                ),
                _common_ctrl(
                    151, "IK_CNS_CTRL_CHANGE_STATE",
                    "IK_CNS_TRIGGER_HIT_OVER", 0, 0, 130, 1,
                    "IK_CNS_CTRL_HAS_CTRL",
                ),
            ]

        elif n == 152:
            row = state_row(152, 151, 0)
            row["has_velset"] = 1
            cs.append(_common_ctrl(
                152, "IK_CNS_CTRL_CHANGE_STATE",
                "IK_CNS_TRIGGER_TIME_EQ", 1, 0, 153, 0,
            ))
            deferred[n] = ["ForceFeedback"]

        elif n == 153:
            row = state_row(153, 151, 0)
            cs += [
                _common_ctrl(
                    153, "IK_CNS_CTRL_HIT_VEL_SET",
                    "IK_CNS_TRIGGER_TIME_EQ", 1, 0, 0, 0,
                    "IK_CNS_CTRL_AXIS_X",
                ),
                _common_ctrl(
                    153, "IK_CNS_CTRL_VEL_SET",
                    "IK_CNS_TRIGGER_HIT_SLIDE_TIME", 0, 0, 0, 0,
                    "IK_CNS_CTRL_AXIS_X",
                ),
                _common_ctrl(
                    153, "IK_CNS_CTRL_CTRL_SET",
                    "IK_CNS_TRIGGER_HIT_CTRL_TIME", 0, 0, 1, 0,
                ),
                _common_ctrl(
                    153, "IK_CNS_CTRL_CHANGE_STATE",
                    "IK_CNS_TRIGGER_HIT_OVER", 0, 0, 131, 1,
                    "IK_CNS_CTRL_HAS_CTRL",
                ),
            ]

        elif n == 154:
            row = state_row(154, 152, 0)
            row["has_velset"] = 1
            cs.append(_common_ctrl(
                154, "IK_CNS_CTRL_CHANGE_STATE",
                "IK_CNS_TRIGGER_TIME_EQ", 1, 0, 155, 0,
            ))
            deferred[n] = ["ForceFeedback"]

        elif n == 155:
            row = state_row(155, 152, 0, land_state=52)
            cs += [
                _common_ctrl(
                    155, "IK_CNS_CTRL_HIT_VEL_SET",
                    "IK_CNS_TRIGGER_TIME_EQ", 1, 0, 0, 0,
                    "IK_CNS_CTRL_AXIS_X | IK_CNS_CTRL_AXIS_Y",
                ),
                _common_ctrl(
                    155, "IK_CNS_CTRL_CTRL_SET",
                    "IK_CNS_TRIGGER_HIT_CTRL_TIME", 0, 0, 1, 0,
                ),
            ]

        elif n == 5000:
            row = state_row(5000, -1, 0)
            row["has_velset"] = 1
            cs += [
                _common_ctrl(
                    5000, "IK_CNS_CTRL_GET_HIT_ANIM",
                    "IK_CNS_TRIGGER_TIME_EQ", 1, 0, 0, 0,
                ),
                _common_ctrl(
                    5000, "IK_CNS_CTRL_CHANGE_STATE",
                    "IK_CNS_TRIGGER_HIT_LAUNCH", 0, 0, 5030, 0,
                ),
                _common_ctrl(
                    5000, "IK_CNS_CTRL_CHANGE_STATE",
                    "IK_CNS_TRIGGER_HIT_NO_LAUNCH", 0, 0, 5001, 0,
                ),
            ]
            deferred[n] = ["ForceFeedback", "StateTypeSet for launch"]

        elif n == 5001:
            row = state_row(5001, -1, 0)
            cs += [
                _common_ctrl(
                    5001, "IK_CNS_CTRL_HIT_VEL_SET",
                    "IK_CNS_TRIGGER_TIME_EQ", 1, 0, 0, 0,
                    "IK_CNS_CTRL_AXIS_X",
                ),
                _common_ctrl(
                    5001, "IK_CNS_CTRL_VEL_MUL",
                    "IK_CNS_TRIGGER_HIT_SLIDE_GE", 0, 0, q8(.6), 0,
                    "IK_CNS_CTRL_AXIS_X",
                ),
                _common_ctrl(
                    5001, "IK_CNS_CTRL_CHANGE_STATE",
                    "IK_CNS_TRIGGER_HIT_OVER", 0, 0, 0, 1,
                    "IK_CNS_CTRL_HAS_CTRL",
                ),
            ]
            deferred[n] = ["DefenceMulSet"]

        elif n == 5010:
            row = state_row(5010, -1, 0)
            row["has_velset"] = 1
            cs += [
                _common_ctrl(
                    5010, "IK_CNS_CTRL_GET_HIT_ANIM",
                    "IK_CNS_TRIGGER_TIME_EQ", 1, 0, 1, 0,
                ),
                _common_ctrl(
                    5010, "IK_CNS_CTRL_CHANGE_STATE",
                    "IK_CNS_TRIGGER_HIT_LAUNCH", 0, 0, 5030, 0,
                ),
                _common_ctrl(
                    5010, "IK_CNS_CTRL_CHANGE_STATE",
                    "IK_CNS_TRIGGER_HIT_NO_LAUNCH", 0, 0, 5011, 0,
                ),
            ]
            deferred[n] = ["ForceFeedback", "StateTypeSet for launch"]

        elif n == 5011:
            row = state_row(5011, -1, 0)
            cs += [
                _common_ctrl(
                    5011, "IK_CNS_CTRL_HIT_VEL_SET",
                    "IK_CNS_TRIGGER_TIME_EQ", 1, 0, 0, 0,
                    "IK_CNS_CTRL_AXIS_X",
                ),
                _common_ctrl(
                    5011, "IK_CNS_CTRL_VEL_MUL",
                    "IK_CNS_TRIGGER_HIT_SLIDE_GE", 0, 0, q8(.6), 0,
                    "IK_CNS_CTRL_AXIS_X",
                ),
                _common_ctrl(
                    5011, "IK_CNS_CTRL_CHANGE_STATE",
                    "IK_CNS_TRIGGER_HIT_OVER", 0, 0, 11, 1,
                    "IK_CNS_CTRL_HAS_CTRL",
                ),
            ]
            deferred[n] = ["DefenceMulSet"]

        elif n == 5020:
            row = state_row(5020, -1, 0)
            row["has_velset"] = 1
            cs += [
                _common_ctrl(
                    5020, "IK_CNS_CTRL_GET_HIT_ANIM",
                    "IK_CNS_TRIGGER_TIME_EQ", 1, 0, 2, 0,
                ),
                _common_ctrl(
                    5020, "IK_CNS_CTRL_CHANGE_STATE",
                    "IK_CNS_TRIGGER_TIME_EQ", 1, 0, 5030, 0,
                ),
            ]
            deferred[n] = ["ForceFeedback"]

        elif n == 5030:
            row = state_row(5030, 5030, 0)
            row["land_level_q8"] = const["air_gethit_groundlevel_q8"]
            cs += [
                _common_ctrl(
                    5030, "IK_CNS_CTRL_HIT_VEL_SET",
                    "IK_CNS_TRIGGER_TIME_EQ", 1, 0, 0, 0,
                    "IK_CNS_CTRL_AXIS_X | IK_CNS_CTRL_AXIS_Y",
                ),
                _common_ctrl(
                    5030, "IK_CNS_CTRL_HIT_RECOVER_STATE",
                    "IK_CNS_TRIGGER_HIT_OVER",
                ),
                _common_ctrl(
                    5030, "IK_CNS_CTRL_CHANGE_STATE",
                    "IK_CNS_TRIGGER_ANIM_END", 0, 0, 5035, 0,
                ),
            ]
            deferred[n] = ["selfAnimExist 5030"]

        elif n == 5035:
            row = state_row(5035, 5035, 0)
            row["land_level_q8"] = const["air_gethit_groundlevel_q8"]
            cs += [
                _common_ctrl(
                    5035, "IK_CNS_CTRL_HIT_RECOVER_STATE",
                    "IK_CNS_TRIGGER_HIT_OVER",
                ),
                _common_ctrl(
                    5035, "IK_CNS_CTRL_HIT_RECOVER_STATE",
                    "IK_CNS_TRIGGER_ANIM_END",
                ),
            ]

        elif n == 5040:
            row = state_row(5040, 5040, 1, land_state=52)
            deferred[n] = ["alive=false -> 5050", "moveTypeSet"]

        elif n == 5050:
            row = state_row(5050, 5050, 0, land_state=5100)
            row["land_level_q8"] = const["air_gethit_groundlevel_q8"]
            cs.append(_common_ctrl(
                5050, "IK_CNS_CTRL_FALL_RECOVERY",
                "IK_CNS_TRIGGER_ALWAYS",
            ))
            deferred[n] = ["fall animation variants"]

        elif n == 5070:
            row = state_row(5070, 5070, 0)
            row["has_velset"] = 1
            cs.append(_common_ctrl(
                5070, "IK_CNS_CTRL_CHANGE_STATE",
                "IK_CNS_TRIGGER_TIME_EQ", 1, 0, 5071, 0,
            ))
            deferred[n] = ["ForceFeedback"]

        elif n == 5071:
            row = state_row(5071, -1, 0, land_state=5110)
            row["land_level_q8"] = const["air_gethit_trip_groundlevel_q8"]
            cs.append(_common_ctrl(
                5071, "IK_CNS_CTRL_HIT_VEL_SET",
                "IK_CNS_TRIGGER_TIME_EQ", 1, 0, 0, 0,
                "IK_CNS_CTRL_AXIS_X | IK_CNS_CTRL_AXIS_Y",
            ))

        elif n == 5080:
            row = state_row(5080, -1, 0)
            row["has_velset"] = 1
            cs.append(_common_ctrl(
                5080, "IK_CNS_CTRL_DOWNED_HIT_BRANCH",
                "IK_CNS_TRIGGER_TIME_EQ", 1,
            ))

        elif n == 5081:
            row = state_row(5081, -1, 0)
            cs += [
                _common_ctrl(
                    5081, "IK_CNS_CTRL_HIT_VEL_SET",
                    "IK_CNS_TRIGGER_TIME_EQ", 1, 0, 0, 0,
                    "IK_CNS_CTRL_AXIS_X",
                ),
                _common_ctrl(
                    5081, "IK_CNS_CTRL_VEL_SET",
                    "IK_CNS_TRIGGER_HIT_OVER", 0, 0, 0, 0,
                    "IK_CNS_CTRL_AXIS_X",
                ),
                _common_ctrl(
                    5081, "IK_CNS_CTRL_CHANGE_STATE",
                    "IK_CNS_TRIGGER_HIT_OVER", 0, 0, 5110, 0,
                ),
            ]

        elif n == 5100:
            row = state_row(5100, 5100, 0)
            cs += [
                _common_ctrl(
                    5100, "IK_CNS_CTRL_POS_SET",
                    "IK_CNS_TRIGGER_TIME_EQ", 1, 0, 0, 0,
                    "IK_CNS_CTRL_AXIS_Y",
                ),
                _common_ctrl(
                    5100, "IK_CNS_CTRL_VEL_SET",
                    "IK_CNS_TRIGGER_TIME_EQ", 1, 0, 0, 0,
                    "IK_CNS_CTRL_AXIS_Y",
                ),
                _common_ctrl(
                    5100, "IK_CNS_CTRL_VEL_MUL",
                    "IK_CNS_TRIGGER_TIME_EQ", 1, 0, q8(.75), 0,
                    "IK_CNS_CTRL_AXIS_X",
                ),
                _common_ctrl(
                    5100, "IK_CNS_CTRL_FALL_GROUND_BRANCH",
                    "IK_CNS_TRIGGER_TIME_EQ", 1, 0, 5110, 0,
                ),
                _common_ctrl(
                    5100, "IK_CNS_CTRL_CHANGE_STATE",
                    "IK_CNS_TRIGGER_ANIM_END", 0, 0, 5101, 0,
                ),
            ]
            deferred[n] = ["FallEnvShake", "HitFallDamage", "ground effect"]

        elif n == 5101:
            row = state_row(5101, 5160, 0, land_state=5110)
            row["air_accel_q8"] = const["down_bounce_yaccel_q8"]
            row["land_level_q8"] = const["down_bounce_groundlevel_q8"]
            cs += [
                _common_ctrl(
                    5101, "IK_CNS_CTRL_FALL_BOUNCE_VEL",
                    "IK_CNS_TRIGGER_TIME_EQ", 1,
                ),
                _common_ctrl(
                    5101, "IK_CNS_CTRL_POS_SET",
                    "IK_CNS_TRIGGER_TIME_EQ", 1, 0, 0,
                    const["down_bounce_offset_y_q8"],
                    "IK_CNS_CTRL_AXIS_Y",
                ),
                _common_ctrl(
                    5101, "IK_CNS_CTRL_POS_ADD",
                    "IK_CNS_TRIGGER_TIME_EQ", 1, 0,
                    const["down_bounce_offset_x_q8"], 0,
                ),
            ]

        elif n == 5110:
            row = state_row(5110, 5110, 0)
            cs += [
                _common_ctrl(
                    5110, "IK_CNS_CTRL_POS_SET",
                    "IK_CNS_TRIGGER_TIME_EQ", 1, 0, 0, 0,
                    "IK_CNS_CTRL_AXIS_Y",
                ),
                _common_ctrl(
                    5110, "IK_CNS_CTRL_VEL_SET",
                    "IK_CNS_TRIGGER_TIME_EQ", 1, 0, 0, 0,
                    "IK_CNS_CTRL_AXIS_Y",
                ),
                _common_ctrl(
                    5110, "IK_CNS_CTRL_VEL_MUL",
                    "IK_CNS_TRIGGER_ALWAYS", 0, 0, q8(.85), 0,
                    "IK_CNS_CTRL_AXIS_X",
                ),
                _common_ctrl(
                    5110, "IK_CNS_CTRL_VEL_SET",
                    "IK_CNS_TRIGGER_ABS_VX_LT_Q8",
                    const["down_friction_threshold_q8"], 0, 0, 0,
                    "IK_CNS_CTRL_AXIS_X",
                ),
                _common_ctrl(
                    5110, "IK_CNS_CTRL_POS_ADD_VEL",
                    "IK_CNS_TRIGGER_ALWAYS", 0, 0, 0, 0,
                    "IK_CNS_CTRL_AXIS_X",
                ),
                _common_ctrl(
                    5110, "IK_CNS_CTRL_CHANGE_STATE",
                    "IK_CNS_TRIGGER_NOT_ALIVE", 0, 0, 5150, 0,
                ),
                _common_ctrl(
                    5110, "IK_CNS_CTRL_CHANGE_STATE",
                    "IK_CNS_TRIGGER_TIME_EQ", const["liedown_time"],
                    0, 5120, 0,
                ),
            ]
            deferred[n] = ["FallEnvShake", "HitFallDamage", "ground effect"]

        elif n == 5120:
            row = state_row(5120, 5120, 0)
            cs += [
                _common_ctrl(
                    5120, "IK_CNS_CTRL_VEL_SET",
                    "IK_CNS_TRIGGER_TIME_EQ", 1, 0, 0, 0,
                    "IK_CNS_CTRL_AXIS_X",
                ),
                _common_ctrl(
                    5120, "IK_CNS_CTRL_CHANGE_STATE",
                    "IK_CNS_TRIGGER_ANIM_END", 0, 0, 0, 1,
                    "IK_CNS_CTRL_HAS_CTRL",
                ),
            ]
            deferred[n] = ["NotHitBy get-up invulnerability", "HitFallSet"]

        elif n == 5150:
            row = state_row(5150, -1, 0, spr=-3)
            cs += [
                _common_ctrl(
                    5150, "IK_CNS_CTRL_VEL_MUL",
                    "IK_CNS_TRIGGER_ALWAYS", 0, 0, q8(.85), 0,
                    "IK_CNS_CTRL_AXIS_X",
                ),
                _common_ctrl(
                    5150, "IK_CNS_CTRL_VEL_SET",
                    "IK_CNS_TRIGGER_ABS_VX_LT_Q8",
                    const["down_friction_threshold_q8"], 0, 0, 0,
                    "IK_CNS_CTRL_AXIS_X",
                ),
                _common_ctrl(
                    5150, "IK_CNS_CTRL_POS_ADD_VEL",
                    "IK_CNS_TRIGGER_ALWAYS", 0, 0, 0, 0,
                    "IK_CNS_CTRL_AXIS_X",
                ),
            ]
            deferred[n] = [
                "NotHitBy defeated invulnerability",
                "MatchOver animation variant",
            ]

        elif n == 5200:
            row = state_row(5200, -1, 0, land_state=5201)
            row["land_level_q8"] = const[
                "air_gethit_groundrecover_groundlevel_q8"
            ]
            cs.append(_common_ctrl(
                5200, "IK_CNS_CTRL_CHANGE_ANIM_IF_END_FROM",
                "IK_CNS_TRIGGER_ALWAYS", 0, 0, 5035, 5050,
            ))

        elif n == 5201:
            row = state_row(5201, 5200, 0, land_state=52)
            cs += [
                _common_ctrl(
                    5201, "IK_CNS_CTRL_VEL_SET",
                    "IK_CNS_TRIGGER_TIME_EQ", 1, 0,
                    const["air_gethit_groundrecover_x_q8"],
                    const["air_gethit_groundrecover_y_q8"],
                    "IK_CNS_CTRL_AXIS_X | IK_CNS_CTRL_AXIS_Y | "
                    "IK_CNS_CTRL_LOCAL_X",
                ),
                _common_ctrl(
                    5201, "IK_CNS_CTRL_POS_SET",
                    "IK_CNS_TRIGGER_TIME_EQ", 1, 0, 0, 0,
                    "IK_CNS_CTRL_AXIS_Y",
                ),
            ]
            deferred[n] = ["Turn by P2Dist", "PalFX", "Explod", "NotHitBy"]

        elif n == 5210:
            row = state_row(5210, 5210, 0, land_state=52)
            row["land_ctrl"] = 1
            row["air_accel_q8"] = const["air_gethit_airrecover_yaccel_q8"]
            row["air_motion_start"] = 4
            cs += [
                _common_ctrl(
                    5210, "IK_CNS_CTRL_VEL_MUL",
                    "IK_CNS_TRIGGER_TIME_EQ", 4, 0,
                    const["air_gethit_airrecover_mul_x_q8"],
                    const["air_gethit_airrecover_mul_y_q8"],
                    "IK_CNS_CTRL_AXIS_X | IK_CNS_CTRL_AXIS_Y",
                ),
                _common_ctrl(
                    5210, "IK_CNS_CTRL_VEL_ADD",
                    "IK_CNS_TRIGGER_TIME_EQ", 4, 0,
                    const["air_gethit_airrecover_add_x_q8"],
                    const["air_gethit_airrecover_add_y_q8"],
                    "IK_CNS_CTRL_AXIS_X | IK_CNS_CTRL_AXIS_Y",
                ),
                _common_ctrl(
                    5210, "IK_CNS_CTRL_VEL_ADD",
                    "IK_CNS_TRIGGER_COMMAND_ACTIVE",
                    "IK_CNS_COMMAND_HOLD_UP", 0, 0,
                    const["air_gethit_airrecover_up_q8"],
                    "IK_CNS_CTRL_AXIS_Y",
                ),
                _common_ctrl(
                    5210, "IK_CNS_CTRL_VEL_ADD",
                    "IK_CNS_TRIGGER_COMMAND_ACTIVE",
                    "IK_CNS_COMMAND_HOLD_DOWN", 0, 0,
                    const["air_gethit_airrecover_down_q8"],
                    "IK_CNS_CTRL_AXIS_Y",
                ),
                _common_ctrl(
                    5210, "IK_CNS_CTRL_VEL_ADD",
                    "IK_CNS_TRIGGER_COMMAND_ACTIVE",
                    "IK_CNS_COMMAND_HOLD_FWD", 0,
                    const["air_gethit_airrecover_fwd_q8"], 0,
                    "IK_CNS_CTRL_AXIS_X | IK_CNS_CTRL_LOCAL_X",
                ),
                _common_ctrl(
                    5210, "IK_CNS_CTRL_VEL_ADD",
                    "IK_CNS_TRIGGER_COMMAND_ACTIVE",
                    "IK_CNS_COMMAND_HOLD_BACK", 0,
                    const["air_gethit_airrecover_back_q8"], 0,
                    "IK_CNS_CTRL_AXIS_X | IK_CNS_CTRL_LOCAL_X",
                ),
                _common_ctrl(
                    5210, "IK_CNS_CTRL_CTRL_SET",
                    "IK_CNS_TRIGGER_TIME_EQ", 20, 0, 1, 0,
                ),
            ]
            deferred[n] = ["Turn by P2Dist", "PalFX", "NotHitBy"]

        elif n == 106:
            row = state_row(106, 47, 0)
            cs += [
                _common_ctrl(
                    106, "IK_CNS_CTRL_VEL_SET",
                    "IK_CNS_TRIGGER_ABS_VX_LT_Q8",
                    const["stand_friction_threshold_q8"], 0, 0, 0,
                    "IK_CNS_CTRL_AXIS_X",
                ),
                _common_ctrl(
                    106, "IK_CNS_CTRL_VEL_SET",
                    "IK_CNS_TRIGGER_TIME_EQ", 1, 0, 0, 0,
                    "IK_CNS_CTRL_AXIS_Y",
                ),
                _common_ctrl(
                    106, "IK_CNS_CTRL_POS_SET",
                    "IK_CNS_TRIGGER_TIME_EQ", 1, 0, 0, 0,
                    "IK_CNS_CTRL_AXIS_Y",
                ),
                _common_ctrl(
                    106, "IK_CNS_CTRL_CHANGE_STATE",
                    "IK_CNS_TRIGGER_TIME_EQ", 7, 0, 0, 1,
                    "IK_CNS_CTRL_HAS_CTRL",
                ),
            ]
            deferred[n] = ["MakeDust at Time=2"]

        row["controller_ofs"] = begin
        row["controller_count"] = len(cs)
        rows.append(row)
        controllers.extend(cs)

    return rows, controllers, deferred


def emit(
    path: Path,
    selected: list[int],
    out_prefix: Path,
    symbol: str,
    common_zss: Path | None = None,
    common_selected: list[int] | None = None,
) -> dict:
    globals_, states = collect_states(parse_sections(path))
    by_number = {state.number: state for state in states}

    missing = [n for n in selected if n not in by_number]
    if missing:
        raise ValueError(f"missing Statedef(s): {missing}")

    const = constants(globals_)
    state_rows: list[dict] = []
    hitdefs: list[dict] = []
    sounds: list[dict] = []
    controllers: list[dict] = []

    for number_ in selected:
        row, hs, ss, cs = parse_state(
            by_number[number_],
            len(hitdefs),
            len(sounds),
            len(controllers),
        )
        state_rows.append(row)
        hitdefs.extend(hs)
        sounds.extend(ss)
        controllers.extend(cs)

    common_deferred: dict[int, list[str]] = {}
    if common_zss is not None and common_selected:
        common_rows, common_ctrls, common_deferred = compile_common_states(
            common_zss, common_selected, const, len(controllers)
        )
        state_rows.extend(common_rows)
        controllers.extend(common_ctrls)

    ident = re.sub(r"[^A-Za-z0-9_]", "_", symbol)
    macro = ident.upper()

    state_lines = [
        "    {"
        f"{r['number']}, {r['anim']}, {r['power_add']}, "
        f"{r['velset_x_q8']}, {r['velset_y_q8']}, "
        f"{r['state_type']}, {r['move_type']}, {r['physics']}, "
        f"{r['ctrl']}, {r['spr_priority']}, {r['has_velset']}u, "
        f"{r['hitdef_ofs']}u, {r['hitdef_count']}u, "
        f"{r['playsnd_ofs']}u, {r['playsnd_count']}u, "
        f"{r['controller_ofs']}u, {r['controller_count']}u, "
        f"{r.get('land_state', 0)}, "
        f"{r.get('air_accel_q8', 0)}, {r.get('land_level_q8', 0)}, "
        f"{r.get('air_motion_start', 0)}u, "
        f"{r.get('land_ctrl', 0)}u, "
        f"{r.get('juggle', 0)}, {r.get('has_juggle', 0)}u, "
        f"{r.get('owns_air_accel', 0)}u, "
        f"{r.get('hitdef_persist', 0)}u"
        "},"
        for r in state_rows
    ]

    hit_lines = [
        "    {"
        f"{h['state_number']}, {h['trigger_kind']}, {h['trigger_value']}, "
        f"{h['damage']}, {h['guard_damage']}, {h['priority']}u, "
        f"{h['pause_p1']}u, {h['pause_p2']}u, "
        f"{h['ground_type']}, {h['ground_slide_time']}u, "
        f"{h['ground_hit_time']}u, {h['air_hit_time']}u, "
        f"{h['ground_velocity_x_q8']}, {h['ground_velocity_y_q8']}, "
        f"{h['air_velocity_x_q8']}, {h['air_velocity_y_q8']}, "
        f"{h['spark_no']}, {h['spark_x']}, {h['spark_y']}, "
        f"{h['hit_sound_group']}, {h['hit_sound_item']}, "
        f"{h['guard_sound_group']}, {h['guard_sound_item']}, "
        f"{h['flags']}, "
        f"{h['guard_flags']}, {h['guard_kill']}u, "
        f"{h['guard_slide_time']}u, "
        f"{h['guard_hit_time']}u, {h['guard_ctrl_time']}u, "
        f"{h['guard_velocity_x_q8']}, "
        f"{h['air_guard_velocity_x_q8']}, "
        f"{h['air_guard_velocity_y_q8']}, "
        f"{h['anim_type']}u, {h['air_anim_type']}u, "
        f"{h['fall_x_velocity_q8']}, {h['fall_y_velocity_q8']}, "
        f"{h['fall_x_velocity_set']}u, {h['fall_recover']}u, "
        f"{h['fall_recover_time']}u, "
        f"{h['down_hit_time']}u, "
        f"{h['down_velocity_x_q8']}, {h['down_velocity_y_q8']}, "
        f"{h['down_bounce']}u, {h['hit_flags']}, "
        f"{h['priority_type']}, {h['air_juggle']}u, "
        f"{h['p1_state_no']}, {h['p2_state_no']}, {h['guard_dist']}, "
        f"{h['p1_facing']}, {h['p2_facing']}, {h['p1_spr_priority']}, "
        f"{h['p2_body_dist_op']}, {h['p2_body_dist_x']}, "
        f"{h['alt_damage']}, {h['alt_damage_prev_state']}, "
        f"{h['trigger2_kind']}, {h['trigger2_value']}, "
        f"{h['yaccel_q8']}"
        "},"
        for h in hitdefs
    ]

    sound_lines = [
        f"    {{{p['state_number']}, {p['trigger_kind']}, "
        f"{p['trigger_value']}, {p['group']}, {p['item']}}},"
        for p in sounds
    ]

    controller_lines = [
        f"    {{{c['state_number']}, {c['type']}, {c['trigger_kind']}, "
        f"{c['trigger_value']}, {c['trigger_value2']}, "
        f"{c['value0']}, {c['value1']}, {c['flags']}}},"
        for c in controllers
    ]

    generated_c = f"""/* Auto-generated by tools/ikemen_cns.py. */
#include "examples/ikemen_saturn/ikemen_cns.h"
#include "{out_prefix.name}.h"

static const ik_cns_state_t {ident}_states[{max(1, len(state_rows))}] = {{
{chr(10).join(state_lines) if state_lines else '    {0},'}
}};

static const ik_cns_hitdef_t {ident}_hitdefs[{max(1, len(hit_lines))}] = {{
{chr(10).join(hit_lines) if hit_lines else '    {0},'}
}};

static const ik_cns_playsnd_t {ident}_playsnds[{max(1, len(sound_lines))}] = {{
{chr(10).join(sound_lines) if sound_lines else '    {0},'}
}};

static const ik_cns_controller_t {ident}_controllers[{max(1, len(controller_lines))}] = {{
{chr(10).join(controller_lines) if controller_lines else '    {0},'}
}};

const ik_cns_asset_t {ident}_cns = {{
    {{
        {const['life']},
        {const['ground_back']}, {const['ground_front']},
        {const['air_back']}, {const['air_front']}, {const['height']},
        {const['attack_dist']},
        {const['walk_fwd_q8']}, {const['walk_back_q8']},
        {const['run_fwd_x_q8']}, {const['run_fwd_y_q8']},
        {const['run_back_x_q8']}, {const['run_back_y_q8']},
        {const['jump_neu_x_q8']}, {const['jump_neu_y_q8']},
        {const['jump_back_q8']}, {const['jump_fwd_q8']},
        {const['run_jump_fwd_x_q8']}, {const['run_jump_fwd_y_q8']},
        {const['air_jump_neu_x_q8']}, {const['air_jump_neu_y_q8']},
        {const['air_jump_back_q8']}, {const['air_jump_fwd_q8']},
        {const['air_jump_num']}, {const['air_jump_height']},
        {const['yaccel_q8']}, {const['stand_friction_q8']},
        {const['crouch_friction_q8']},
        {const['stand_friction_threshold_q8']},
        {const['crouch_friction_threshold_q8']},
        {const['liedown_time']},
        {const['air_gethit_groundlevel_q8']},
        {const['air_gethit_trip_groundlevel_q8']},
        {const['down_bounce_offset_x_q8']},
        {const['down_bounce_offset_y_q8']},
        {const['down_bounce_yaccel_q8']},
        {const['down_bounce_groundlevel_q8']},
        {const['down_friction_threshold_q8']},
        {const['air_gethit_groundrecover_x_q8']},
        {const['air_gethit_groundrecover_y_q8']},
        {const['air_gethit_groundrecover_threshold_q8']},
        {const['air_gethit_groundrecover_groundlevel_q8']},
        {const['air_gethit_airrecover_mul_x_q8']},
        {const['air_gethit_airrecover_mul_y_q8']},
        {const['air_gethit_airrecover_add_x_q8']},
        {const['air_gethit_airrecover_add_y_q8']},
        {const['air_gethit_airrecover_back_q8']},
        {const['air_gethit_airrecover_fwd_q8']},
        {const['air_gethit_airrecover_up_q8']},
        {const['air_gethit_airrecover_down_q8']},
        {const['air_gethit_airrecover_threshold_q8']},
        {const['air_gethit_airrecover_yaccel_q8']},
        {const['air_juggle']}
    }},
    {ident}_states, {len(state_rows)}u,
    {ident}_hitdefs, {len(hitdefs)}u,
    {ident}_playsnds, {len(sounds)}u,
    {ident}_controllers, {len(controllers)}u
}};
"""

    generated_h = f"""/* Auto-generated by tools/ikemen_cns.py. */
#pragma once

#include "examples/ikemen_saturn/ikemen_cns.h"

#define {macro}_CNS_STATE_COUNT {len(state_rows)}u
#define {macro}_CNS_HITDEF_COUNT {len(hitdefs)}u
#define {macro}_CNS_PLAYSND_COUNT {len(sounds)}u
#define {macro}_CNS_CONTROLLER_COUNT {len(controllers)}u

extern const ik_cns_asset_t {ident}_cns;
"""

    out_prefix.parent.mkdir(parents=True, exist_ok=True)
    out_prefix.with_suffix(".c").write_text(generated_c, encoding="utf-8")
    out_prefix.with_suffix(".h").write_text(generated_h, encoding="utf-8")

    report = {
        "states": state_rows,
        "constants": const,
        "hitdefs": hitdefs,
        "playsnds": sounds,
        "controllers": controllers,
        "common_deferred": common_deferred,
    }
    out_prefix.with_suffix(".json").write_text(
        json.dumps(report, indent=2),
        encoding="utf-8",
    )
    return report


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--cns", required=True, type=Path)
    parser.add_argument("--states", required=True)
    parser.add_argument("--common-zss", type=Path)
    parser.add_argument("--common-states", default="")
    parser.add_argument("--out-prefix", required=True, type=Path)
    parser.add_argument("--symbol", required=True)
    args = parser.parse_args(argv)

    selected = [
        int(value.strip())
        for value in args.states.split(",")
        if value.strip()
    ]

    common_selected = [
        int(value.strip())
        for value in args.common_states.split(",")
        if value.strip()
    ]

    report = emit(
        args.cns,
        selected,
        args.out_prefix,
        args.symbol,
        args.common_zss,
        common_selected,
    )

    unsupported = {
        row["number"]: row["unsupported_controllers"]
        for row in report["states"]
        if row["unsupported_controllers"]
    }

    print(
        f"[ikemen_cns] {args.symbol}: "
        f"states={len(report['states'])} "
        f"hitdefs={len(report['hitdefs'])} "
        f"playsnds={len(report['playsnds'])} "
        f"controllers={len(report['controllers'])}"
    )

    if unsupported:
        print(f"[ikemen_cns] deferred controllers: {unsupported}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
