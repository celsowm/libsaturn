#!/usr/bin/env python3
"""Compile the deterministic KFM CNS subset needed by the Saturn runtime.

This is intentionally an offline compiler, not a text parser on the Saturn.
It emits:
  * [Data]/[Size]/[Velocity]/[Movement] constants in Q8.8 where fractional;
  * selected Statedef metadata;
  * HitDef controllers with simple Time/AnimElem activation;
  * PlaySnd controllers with simple Time/AnimElem activation.

Unsupported expressions in selected states fail loudly instead of silently
changing game behavior. More controller kinds can be added without changing
the generated asset ABI.
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


def collect_states(sections: list[Section]) -> tuple[dict[str, Section], list[State]]:
    globals_: dict[str, Section] = {}
    states: list[State] = []
    current: State | None = None

    for section in sections:
        m = re.fullmatch(r"Statedef\s+(-?\d+)", section.name, flags=re.I)
        if m:
            current = State(int(m.group(1)), section, [])
            states.append(current)
            continue
        if re.match(r"State\s+", section.name, flags=re.I):
            if current is not None:
                current.controllers.append(section)
            continue
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


def pair(text: str | None, default_x: float = 0.0,
         default_y: float = 0.0) -> tuple[float, float]:
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


def sound_pair(text: str | None, default=(-1, -1)) -> tuple[int, int]:
    if text is None:
        return default
    value = text.strip()
    if value[:1].lower() == "s":
        value = value[1:]
    parts = [p.strip() for p in value.split(",")]
    if len(parts) != 2:
        raise ValueError(f"expected sound group,item, got {text!r}")
    return integer(parts[0]), integer(parts[1])


def trigger(section: Section) -> tuple[str, int]:
    triggers = section.all("trigger1")
    if not triggers:
        return "IK_CNS_TRIGGER_ALWAYS", 0
    if len(triggers) != 1:
        raise ValueError(
            f"[{section.name}] selected subset requires one trigger1, got {triggers!r}"
        )
    expr = triggers[0].strip()
    m = re.fullmatch(r"Time\s*=\s*(-?\d+)", expr, flags=re.I)
    if m:
        return "IK_CNS_TRIGGER_TIME_EQ", int(m.group(1))
    m = re.fullmatch(r"AnimElem\s*=\s*(\d+)", expr, flags=re.I)
    if m:
        return "IK_CNS_TRIGGER_ANIM_ELEM_EQ", int(m.group(1))
    raise ValueError(f"[{section.name}] unsupported trigger expression {expr!r}")


def constants(globals_: dict[str, Section]) -> dict[str, int]:
    data = globals_.get("data", Section("Data"))
    size = globals_.get("size", Section("Size"))
    vel = globals_.get("velocity", Section("Velocity"))
    movement = globals_.get("movement", Section("Movement"))

    run_fwd = pair(vel.get("run.fwd"))
    run_back = pair(vel.get("run.back"))
    jump_neu = pair(vel.get("jump.neu"))

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


def parse_state(state: State, hitdef_ofs: int, sound_ofs: int):
    sd = state.statedef
    state_type = STATE_TYPE.get((sd.get("type", "U") or "U").strip().upper())
    move_type = MOVE_TYPE.get((sd.get("movetype", "U") or "U").strip().upper())
    physics = PHYSICS.get((sd.get("physics", "N") or "N").strip().upper())
    if state_type is None or move_type is None or physics is None:
        raise ValueError(f"state {state.number}: unsupported Statedef type")

    vx, vy = pair(sd.get("velset"))
    has_velset = sd.get("velset") is not None

    hitdefs = []
    sounds = []
    unsupported = []
    for ctrl in state.controllers:
        ctype = (ctrl.get("type", "") or "").strip().lower()
        if ctype == "hitdef":
            trig_kind, trig_value = trigger(ctrl)
            damage, guard_damage = pair(ctrl.get("damage"), 0, 0)
            pause1, pause2 = pair(ctrl.get("pausetime"), 0, 0)
            gx, gy = pair(ctrl.get("ground.velocity"), 0, 0)
            ax, ay = pair(ctrl.get("air.velocity"), gx, gy)
            sparkx, sparky = pair(ctrl.get("sparkxy"), 0, 0)
            hs = sound_pair(ctrl.get("hitsound"))
            gs = sound_pair(ctrl.get("guardsound"))
            flags = []
            if integer(ctrl.get("fall"), 0):
                flags.append("IK_CNS_HITDEF_FALL")
            if integer(ctrl.get("forcenofall"), 0):
                flags.append("IK_CNS_HITDEF_FORCE_NO_FALL")
            hitdefs.append({
                "state_number": state.number,
                "trigger_kind": trig_kind,
                "trigger_value": trig_value,
                "damage": int(damage),
                "guard_damage": int(guard_damage),
                "priority": integer((ctrl.get("priority") or "4").split(",", 1)[0], 4),
                "pause_p1": int(pause1),
                "pause_p2": int(pause2),
                "ground_type": GROUND_TYPE.get(
                    (ctrl.get("ground.type", "normal") or "normal").strip().lower(),
                    "IK_CNS_GROUND_NORMAL",
                ),
                "ground_slide_time": integer(ctrl.get("ground.slidetime"), 0),
                "ground_hit_time": integer(ctrl.get("ground.hittime"), 0),
                "air_hit_time": integer(
                    ctrl.get("air.hittime"), integer(ctrl.get("ground.hittime"), 0)
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
            })
        elif ctype == "playsnd":
            trig_kind, trig_value = trigger(ctrl)
            group, item = sound_pair(ctrl.get("value"))
            sounds.append({
                "state_number": state.number,
                "trigger_kind": trig_kind,
                "trigger_value": trig_value,
                "group": group,
                "item": item,
            })
        elif ctype:
            unsupported.append(ctype)

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
        "unsupported_controllers": sorted(set(unsupported)),
    }
    return state_row, hitdefs, sounds


def emit(path: Path, selected: list[int], out_prefix: Path, symbol: str) -> dict:
    sections = parse_sections(path)
    globals_, states = collect_states(sections)
    by_number = {state.number: state for state in states}

    missing = [n for n in selected if n not in by_number]
    if missing:
        raise ValueError(f"missing Statedef(s): {missing}")

    const = constants(globals_)
    state_rows = []
    hitdefs = []
    sounds = []
    for number_ in selected:
        row, hs, ss = parse_state(by_number[number_], len(hitdefs), len(sounds))
        state_rows.append(row)
        hitdefs.extend(hs)
        sounds.extend(ss)

    ident = re.sub(r"[^A-Za-z0-9_]", "_", symbol)
    macro = ident.upper()

    state_lines = [
        "    {"
        f"{r['number']}, {r['anim']}, {r['power_add']}, "
        f"{r['velset_x_q8']}, {r['velset_y_q8']}, "
        f"{r['state_type']}, {r['move_type']}, {r['physics']}, "
        f"{r['ctrl']}, {r['spr_priority']}, {r['has_velset']}u, "
        f"{r['hitdef_ofs']}u, {r['hitdef_count']}u, "
        f"{r['playsnd_ofs']}u, {r['playsnd_count']}u"
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
        "    {"
        f"{p['state_number']}, {p['trigger_kind']}, {p['trigger_value']}, "
        f"{p['group']}, {p['item']}"
        "},"
        for p in sounds
    ]

    c = f"""/* Auto-generated by tools/ikemen_cns.py. */
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
        {const['yaccel_q8']}, {const['stand_friction_q8']},
        {const['crouch_friction_q8']},
        {const['stand_friction_threshold_q8']},
        {const['crouch_friction_threshold_q8']}
    }},
    {ident}_states, {len(state_rows)}u,
    {ident}_hitdefs, {len(hitdefs)}u,
    {ident}_playsnds, {len(sounds)}u
}};
"""

    h = f"""/* Auto-generated by tools/ikemen_cns.py. */
#pragma once

#include "examples/ikemen_saturn/ikemen_cns.h"

#define {macro}_CNS_STATE_COUNT {len(state_rows)}u
#define {macro}_CNS_HITDEF_COUNT {len(hitdefs)}u
#define {macro}_CNS_PLAYSND_COUNT {len(sounds)}u

extern const ik_cns_asset_t {ident}_cns;
"""

    out_prefix.parent.mkdir(parents=True, exist_ok=True)
    out_prefix.with_suffix(".c").write_text(c, encoding="utf-8")
    out_prefix.with_suffix(".h").write_text(h, encoding="utf-8")

    report = {
        "states": state_rows,
        "constants": const,
        "hitdefs": hitdefs,
        "playsnds": sounds,
    }
    out_prefix.with_suffix(".json").write_text(
        json.dumps(report, indent=2), encoding="utf-8"
    )
    return report


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--cns", required=True, type=Path)
    parser.add_argument("--states", required=True)
    parser.add_argument("--out-prefix", required=True, type=Path)
    parser.add_argument("--symbol", required=True)
    args = parser.parse_args(argv)

    selected = [int(v.strip()) for v in args.states.split(",") if v.strip()]
    report = emit(args.cns, selected, args.out_prefix, args.symbol)
    unsupported = {
        row["number"]: row["unsupported_controllers"]
        for row in report["states"] if row["unsupported_controllers"]
    }
    print(
        f"[ikemen_cns] {args.symbol}: states={len(report['states'])} "
        f"hitdefs={len(report['hitdefs'])} playsnds={len(report['playsnds'])}"
    )
    if unsupported:
        print(f"[ikemen_cns] deferred controllers: {unsupported}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
