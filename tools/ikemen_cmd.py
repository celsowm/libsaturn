#!/usr/bin/env python3
"""Compile an Ikemen/MUGEN CMD file into bounded Saturn command tables.

The parser intentionally mirrors the command grammar used by Ikemen GO:
comma-separated steps, + chords, | alternatives, / hold, ~ release/charge,
$ four-way directions, > strict steps, duplicate-direction auto-greater
expansion, per-command timing and buffering, and duplicate command names.

Runtime matching lives in examples/ikemen_saturn/ikemen_command.c.
"""

from __future__ import annotations

import argparse
import json
import re
from dataclasses import dataclass, field
from pathlib import Path

KEYS = {
    "U": "IK_CMD_KEY_U", "D": "IK_CMD_KEY_D",
    "B": "IK_CMD_KEY_B", "F": "IK_CMD_KEY_F",
    "L": "IK_CMD_KEY_L", "R": "IK_CMD_KEY_R",
    "UB": "IK_CMD_KEY_UB", "UF": "IK_CMD_KEY_UF",
    "DB": "IK_CMD_KEY_DB", "DF": "IK_CMD_KEY_DF",
    "UL": "IK_CMD_KEY_UL", "UR": "IK_CMD_KEY_UR",
    "DL": "IK_CMD_KEY_DL", "DR": "IK_CMD_KEY_DR",
    "N": "IK_CMD_KEY_N",
    "a": "IK_CMD_KEY_A", "b": "IK_CMD_KEY_BTN_B",
    "c": "IK_CMD_KEY_C", "x": "IK_CMD_KEY_X",
    "y": "IK_CMD_KEY_Y", "z": "IK_CMD_KEY_Z",
    "s": "IK_CMD_KEY_S", "d": "IK_CMD_KEY_BTN_D",
    "w": "IK_CMD_KEY_W", "m": "IK_CMD_KEY_M",
}

DIRECTIONS = set("U D B F L R UB UF DB DF UL UR DL DR N".split())


@dataclass
class Key:
    name: str
    slash: bool = False
    tilde: bool = False
    dollar: bool = False
    charge: int = 0


@dataclass
class Step:
    keys: list[Key] = field(default_factory=list)
    greater: bool = False
    or_logic: bool = False


@dataclass
class Pattern:
    name: str
    steps: list[Step]
    time: int
    buffer: int
    step_time: int
    buffer_hitpause: bool
    loop_order: list[int] = field(default_factory=list)


@dataclass
class CmdFile:
    patterns: list[Pattern]
    names: list[str]


def _strip_comment(line: str) -> str:
    return line.split(";", 1)[0].strip()


def _unquote(value: str) -> str:
    value = value.strip()
    if len(value) >= 2 and value[0] == value[-1] == '"':
        return value[1:-1]
    return value


def _bool(value: str, default: bool) -> bool:
    value = value.strip().lower()
    if value in ("1", "true", "yes"): return True
    if value in ("0", "false", "no"): return False
    return default


def _sections(path: Path) -> list[tuple[str, dict[str, str]]]:
    text = path.read_text(encoding="utf-8-sig", errors="strict")
    out: list[tuple[str, dict[str, str]]] = []
    name: str | None = None
    values: dict[str, str] = {}
    for raw in text.splitlines():
        line = _strip_comment(raw)
        if not line:
            continue
        if line.startswith("[") and line.endswith("]"):
            if name is not None:
                out.append((name, values))
            name = line[1:-1].strip()
            values = {}
            continue
        if name is not None and "=" in line:
            key, value = line.split("=", 1)
            values[key.strip().lower()] = value.strip()
    if name is not None:
        out.append((name, values))
    return out


def _parse_part(text: str, remap: dict[str, str]) -> tuple[Key, bool]:
    text = text.strip()
    greater = False
    slash = tilde = dollar = False
    charge = 0
    while text and text[0] in ">/~$":
        c = text[0]
        text = text[1:].lstrip()
        if c == ">":
            greater = True
        elif c == "/":
            slash = True
            m = re.match(r"(\d+)", text)
            if m:
                charge = int(m.group(1))
                text = text[len(m.group(1)):]
        elif c == "~":
            tilde = True
            m = re.match(r"(\d+)", text)
            if m:
                charge = int(m.group(1))
                text = text[len(m.group(1)):]
        elif c == "$":
            dollar = True

    token = text.strip()
    if token in remap:
        token = remap[token]
    if token not in KEYS:
        raise ValueError(f"unsupported CMD key {token!r}")
    return Key(token, slash, tilde, dollar, charge), greater


def _is_single_direction(step: Step) -> bool:
    return len(step.keys) == 1 and step.keys[0].name in DIRECTIONS


def _equal_steps(a: Step, b: Step) -> bool:
    return a == b


def _is_dir_to_button(a: Step, b: Step) -> bool:
    if any(k.slash for k in b.keys):
        return False
    if any(k.name not in DIRECTIONS for k in a.keys):
        return False
    if any(x.name == y.name for x in a.keys for y in b.keys):
        return False
    if any(k.name not in DIRECTIONS and not k.tilde for k in b.keys):
        return True
    if any(k.name in DIRECTIONS and k.tilde for k in a.keys):
        return any(not k.tilde for k in b.keys)
    return False


def _auto_greater(steps: list[Step], enabled: bool) -> list[Step]:
    if not enabled or len(steps) < 2:
        return steps
    out = [steps[0]]
    for prev, cur in zip(steps, steps[1:]):
        if _is_single_direction(prev) and _is_single_direction(cur) and _equal_steps(prev, cur):
            key = cur.keys[0]
            out.append(Step(
                keys=[Key(key.name, False, not key.tilde, key.dollar, 0)],
                greater=True,
            ))
            out.append(Step(
                keys=[Key(key.name, key.slash, key.tilde, key.dollar, key.charge)],
                greater=True,
            ))
        else:
            out.append(cur)
    return out


def _loop_order(steps: list[Step]) -> list[int]:
    order: list[int] = []
    i = len(steps) - 1
    while i >= 0:
        if i > 0 and _is_dir_to_button(steps[i - 1], steps[i]):
            start, end = i - 1, i
            while start > 0 and _is_dir_to_button(steps[start - 1], steps[start]):
                start -= 1
            order.extend(range(start, end + 1))
            i = start - 1
        else:
            order.append(i)
            i -= 1
    return order


def parse_cmd(path: Path) -> CmdFile:
    sections = _sections(path)
    defaults = {
        "time": 15,
        "steptime": -1,
        "autogreater": True,
        "buffer": 1,
        "hitpause": True,
    }
    remap = {k: k for k in "abcxyzsdwm"}
    raw_commands: list[dict[str, str]] = []

    for section, values in sections:
        low = section.lower()
        if low == "remap":
            for old in list(remap):
                if old in values:
                    remap[old] = _unquote(values[old])
        elif low == "defaults":
            if "command.time" in values:
                defaults["time"] = max(1, int(values["command.time"]))
            if "command.steptime" in values:
                defaults["steptime"] = int(values["command.steptime"])
            if "command.autogreater" in values:
                defaults["autogreater"] = _bool(values["command.autogreater"], True)
            if "command.buffer.time" in values:
                defaults["buffer"] = max(1, int(values["command.buffer.time"]))
            if "command.buffer.hitpause" in values:
                defaults["hitpause"] = _bool(values["command.buffer.hitpause"], True)
        elif low == "command":
            raw_commands.append(values)

    patterns: list[Pattern] = []
    name_order: list[str] = []
    for values in raw_commands:
        name = _unquote(values.get("name", ""))
        expr = values.get("command", "").strip()
        if not name or not expr:
            continue
        if name not in name_order:
            name_order.append(name)

        raw_steps = [part.strip() for part in expr.split(",")]
        steps: list[Step] = []
        for raw_step in raw_steps:
            if not raw_step:
                raise ValueError(f"{name}: empty command step")
            if "+" in raw_step and "|" in raw_step:
                raise ValueError(f"{name}: cannot mix + and | in a command step")
            or_logic = "|" in raw_step
            parts = raw_step.split("|" if or_logic else "+")
            keys: list[Key] = []
            greater = False
            for part in parts:
                key, part_greater = _parse_part(part, remap)
                keys.append(key)
                greater = greater or part_greater
            if not or_logic and any(k.slash for k in keys):
                # Ikemen's legacy compatibility propagates / over an AND chord.
                for key in keys:
                    key.slash = True
            steps.append(Step(keys, greater, or_logic))

        steps = _auto_greater(steps, bool(defaults["autogreater"]))
        if len(steps) > 16:
            raise ValueError(f"{name}: {len(steps)} steps exceed Saturn runtime limit 16")
        loop = _loop_order(steps)

        time = max(1, int(values.get("time", defaults["time"])))
        step_time = int(values.get("steptime", defaults["steptime"]))
        if step_time < 0:
            step_time = time
        buffer = max(1, int(values.get("buffer.time", defaults["buffer"])))
        hitpause = _bool(values.get("buffer.hitpause", str(defaults["hitpause"])),
                         bool(defaults["hitpause"]))

        hold_only = all(k.slash for step in steps for k in step.keys)
        if hold_only:
            buffer = 1

        patterns.append(Pattern(name, steps, time, buffer, step_time, hitpause, loop))

    if len(patterns) > 64:
        raise ValueError(f"{len(patterns)} command patterns exceed Saturn runtime limit 64")
    return CmdFile(patterns, name_order)


def _ident(text: str) -> str:
    value = re.sub(r"[^A-Za-z0-9_]", "_", text)
    if not value or value[0].isdigit():
        value = "_" + value
    return value


def emit(asset: CmdFile, out_prefix: Path, symbol: str) -> None:
    symbol = _ident(symbol)
    grouped: list[Pattern] = []
    name_meta: list[tuple[str, int, int]] = []
    for name in asset.names:
        variants = [p for p in asset.patterns if p.name == name]
        first = len(grouped)
        grouped.extend(variants)
        name_meta.append((name, first, len(variants)))

    keys: list[Key] = []
    steps: list[tuple[int, int, Step]] = []
    loops: list[int] = []
    patterns: list[tuple[Pattern, int, int, int]] = []
    name_ids = {name: i for i, name in enumerate(asset.names)}

    for p in grouped:
        step_ofs = len(steps)
        for step in p.steps:
            key_ofs = len(keys)
            keys.extend(step.keys)
            steps.append((key_ofs, len(step.keys), step))
        loop_ofs = len(loops)
        loops.extend(p.loop_order)
        patterns.append((p, step_ofs, loop_ofs, name_ids[p.name]))

    key_lines = []
    for k in keys:
        flags = []
        if k.slash: flags.append("IK_CMD_KEY_SLASH")
        if k.tilde: flags.append("IK_CMD_KEY_TILDE")
        if k.dollar: flags.append("IK_CMD_KEY_DOLLAR")
        f = " | ".join(flags) if flags else "0u"
        key_lines.append(f"    {{{KEYS[k.name]}, {f}, {k.charge}u, 0u}},")

    step_lines = []
    for key_ofs, key_count, step in steps:
        flags = []
        if step.greater: flags.append("IK_CMD_STEP_GREATER")
        if step.or_logic: flags.append("IK_CMD_STEP_OR")
        f = " | ".join(flags) if flags else "0u"
        step_lines.append(f"    {{{key_ofs}u, {key_count}u, {f}}},")

    pattern_lines = []
    for p, step_ofs, loop_ofs, name_id in patterns:
        flags = "IK_CMD_PATTERN_BUFFER_HITPAUSE" if p.buffer_hitpause else "0u"
        pattern_lines.append(
            f"    {{{step_ofs}u, {loop_ofs}u, {name_id}u, {len(p.steps)}u, "
            f"{p.time}u, {p.step_time}u, {p.buffer}u, {flags}, 0u}},"
        )

    name_lines = [
        f'    {{"{name}", {first}u, {count}u, 0u}},'
        for name, first, count in name_meta
    ]

    macro_lines = [
        f"#define {symbol.upper()}_CMD_{_ident(name).upper()} {i}u"
        for i, name in enumerate(asset.names)
    ]

    c = f"""/* Auto-generated by tools/ikemen_cmd.py. */
#include "examples/ikemen_saturn/ikemen_command.h"
#include "{out_prefix.name}.h"

static const ik_cmd_key_spec_t {symbol}_keys[{max(1, len(keys))}] = {{
{chr(10).join(key_lines) if key_lines else '    {0u, 0u, 0u, 0u},'}
}};

static const ik_cmd_step_t {symbol}_steps[{max(1, len(steps))}] = {{
{chr(10).join(step_lines) if step_lines else '    {0u, 0u, 0u},'}
}};

static const uint8_t {symbol}_loop_order[{max(1, len(loops))}] = {{
    {", ".join(str(v) + "u" for v in loops) if loops else "0u"}
}};

static const ik_cmd_pattern_t {symbol}_patterns[{max(1, len(patterns))}] = {{
{chr(10).join(pattern_lines) if pattern_lines else '    {0u, 0u, 0u, 0u, 1u, 1u, 1u, 0u, 0u},'}
}};

static const ik_cmd_name_t {symbol}_names[{max(1, len(name_meta))}] = {{
{chr(10).join(name_lines) if name_lines else '    {"", 0u, 0u, 0u},'}
}};

const ik_command_asset_t {symbol}_commands = {{
    {symbol}_keys, {len(keys)}u,
    {symbol}_steps, {len(steps)}u,
    {symbol}_loop_order, {len(loops)}u,
    {symbol}_patterns, {len(patterns)}u,
    {symbol}_names, {len(name_meta)}u
}};
"""

    h = f"""/* Auto-generated by tools/ikemen_cmd.py. */
#pragma once

#include "examples/ikemen_saturn/ikemen_command.h"

#define {symbol.upper()}_COMMAND_NAME_COUNT {len(name_meta)}u
#define {symbol.upper()}_COMMAND_PATTERN_COUNT {len(patterns)}u
{chr(10).join(macro_lines)}

extern const ik_command_asset_t {symbol}_commands;
"""

    out_prefix.parent.mkdir(parents=True, exist_ok=True)
    out_prefix.with_suffix(".c").write_text(c, encoding="utf-8")
    out_prefix.with_suffix(".h").write_text(h, encoding="utf-8")
    out_prefix.with_suffix(".json").write_text(json.dumps({
        "names": asset.names,
        "name_count": len(name_meta),
        "pattern_count": len(patterns),
        "step_count": len(steps),
        "key_count": len(keys),
        "patterns": [
            {
                "name": p.name,
                "time": p.time,
                "buffer": p.buffer,
                "steps": len(p.steps),
                "loop_order": p.loop_order,
            }
            for p in grouped
        ],
    }, indent=2), encoding="utf-8")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--cmd", required=True, type=Path)
    parser.add_argument("--out-prefix", required=True, type=Path)
    parser.add_argument("--symbol", required=True)
    args = parser.parse_args(argv)

    asset = parse_cmd(args.cmd)
    emit(asset, args.out_prefix, args.symbol)
    print(
        f"[ikemen_cmd] {args.symbol}: names={len(asset.names)} "
        f"patterns={len(asset.patterns)}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
