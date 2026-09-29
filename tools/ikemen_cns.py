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


def sound_pair(
    text: str | None,
    default: tuple[int, int] = (-1, -1),
) -> tuple[int, int]:
    if text is None:
        return default

    value = text.strip()
    if value[:1].lower() == "s":
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

    m = re.fullmatch(r"Time\s*=\s*(-?\d+)", expr, flags=re.I)
    if m:
        return "IK_CNS_TRIGGER_TIME_EQ", int(m.group(1)), 0

    m = re.fullmatch(r"AnimElem\s*=\s*(\d+)", expr, flags=re.I)
    if m:
        return "IK_CNS_TRIGGER_ANIM_ELEM_EQ", int(m.group(1)), 0

    if re.fullmatch(r"AnimTime\s*=\s*0", expr, flags=re.I):
        return "IK_CNS_TRIGGER_ANIM_END", 0, 0

    raise ValueError(f"[{section.name}] unsupported trigger expression {expr!r}")


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

    if ctype == "width" and len(triggers) == 1:
        parsed = _anim_elem_range_trigger(triggers[0])
        if parsed is not None:
            return "IK_CNS_TRIGGER_ANIM_ELEM_RANGE", parsed[0], parsed[1]

    if ctype == "changeanim":
        parsed = _move_contact_window(triggers)
        if parsed is not None:
            return (
                "IK_CNS_TRIGGER_MOVE_CONTACT_ELEM_WINDOW",
                parsed[0],
                parsed[1],
            )

    return simple_trigger(ctrl)


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

    return {
        "life": integer(data.get("life"), 1000),
        "ground_back": integer(size.get("ground.back"), 15),
        "ground_front": integer(size.get("ground.front"), 16),
        "air_back": integer(size.get("air.back"), 12),
        "air_front": integer(size.get("air.front"), 12),
        "height": integer(size.get("height"), 60),
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
        "sprpriority",
        "changeanim",
        "width",
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
        x = number(ctrl.get("x"), 0)
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

    width_front, width_back = pair(ctrl.get("value"), 0, 0)
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
            trig_kind, trig_value, _ = simple_trigger(ctrl)

            damage, guard_damage = pair(ctrl.get("damage"), 0, 0)
            pause1, pause2 = pair(ctrl.get("pausetime"), 0, 0)
            gx, gy = pair(ctrl.get("ground.velocity"), 0, 0)
            ax, ay = pair(ctrl.get("air.velocity"), gx, gy)
            sparkx, sparky = pair(ctrl.get("sparkxy"), 0, 0)
            hs = sound_pair(ctrl.get("hitsound"))
            gs = sound_pair(ctrl.get("guardsound"))

            flags: list[str] = []
            if integer(ctrl.get("fall"), 0):
                flags.append("IK_CNS_HITDEF_FALL")
            if integer(ctrl.get("forcenofall"), 0):
                flags.append("IK_CNS_HITDEF_FORCE_NO_FALL")

            hitdefs.append(
                {
                    "state_number": state.number,
                    "trigger_kind": trig_kind,
                    "trigger_value": trig_value,
                    "damage": int(damage),
                    "guard_damage": int(guard_damage),
                    "priority": integer(
                        (ctrl.get("priority") or "4").split(",", 1)[0],
                        4,
                    ),
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
        "anim": integer(sd.get("anim"), state.number),
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

    supported = {0, 10, 11, 12, 20, 40, 45, 50, 51, 52, 100, 105, 106}
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
        return {
            "number": n,
            "anim": anim,
            "power_add": 0,
            "velset_x_q8": 0,
            "velset_y_q8": 0,
            "state_type": stype,
            "move_type": "IK_CNS_MOVE_IDLE",
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
            ]
            deferred[n] = ["alive=false -> state 5050"]

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
        f"{r.get('land_state', 0)}"
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
        f"{h['flags']}"
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
        {const['crouch_friction_threshold_q8']}
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
