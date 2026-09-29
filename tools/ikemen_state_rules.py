#!/usr/bin/env python3
"""Compile selected MUGEN/Ikemen [State -1] ChangeState rules to postfix bytecode."""

from __future__ import annotations

import argparse
import json
import re
from dataclasses import dataclass
from pathlib import Path

from tools.ikemen_cmd import parse_cmd

TOKEN_RE = re.compile(
    r'\s*(?:(?P<string>"[^"]*")|(?P<number>-?\d+)|'
    r'(?P<op>\|\||&&|!=|>=|<=|=|>|<|\(|\)|\[|\]|,)|'
    r'(?P<ident>[A-Za-z_][A-Za-z0-9_.]*))'
)
STATE_TYPE_VALUE = {"S": 1, "C": 2, "A": 3, "L": 4}
OP = {
    "command_active": "IK_CMD_RULE_COMMAND_ACTIVE",
    "command_inactive": "IK_CMD_RULE_COMMAND_INACTIVE",
    "state_type_eq": "IK_CMD_RULE_STATE_TYPE_EQ",
    "state_type_ne": "IK_CMD_RULE_STATE_TYPE_NE",
    "state_no_eq": "IK_CMD_RULE_STATE_NO_EQ",
    "state_no_ne": "IK_CMD_RULE_STATE_NO_NE",
    "state_no_range": "IK_CMD_RULE_STATE_NO_RANGE",
    "state_time_eq": "IK_CMD_RULE_STATE_TIME_EQ",
    "state_time_gt": "IK_CMD_RULE_STATE_TIME_GT",
    "state_time_ge": "IK_CMD_RULE_STATE_TIME_GE",
    "state_time_lt": "IK_CMD_RULE_STATE_TIME_LT",
    "state_time_le": "IK_CMD_RULE_STATE_TIME_LE",
    "ctrl": "IK_CMD_RULE_CTRL",
    "move_contact": "IK_CMD_RULE_MOVE_CONTACT",
    "not": "IK_CMD_RULE_NOT",
    "and": "IK_CMD_RULE_AND",
    "or": "IK_CMD_RULE_OR",
}


@dataclass(frozen=True)
class Insn:
    op: str
    a: int = 0
    b: int = 0


@dataclass
class Rule:
    label: str
    target: int
    code: list[Insn]


def _strip_comment(line: str) -> str:
    return line.split(";", 1)[0].strip()


def _state_sections(path: Path) -> list[tuple[str, list[tuple[str, str]]]]:
    text = path.read_text(encoding="utf-8-sig", errors="strict")
    sections: list[tuple[str, list[tuple[str, str]]]] = []
    header: str | None = None
    pairs: list[tuple[str, str]] = []
    for raw in text.splitlines():
        line = _strip_comment(raw)
        if not line:
            continue
        if line.startswith("[") and line.endswith("]"):
            if header is not None:
                sections.append((header, pairs))
            header = line[1:-1].strip()
            pairs = []
            continue
        if header is not None and "=" in line:
            key, value = line.split("=", 1)
            pairs.append((key.strip().lower(), value.strip()))
    if header is not None:
        sections.append((header, pairs))
    return sections


def _tokenize(expr: str) -> list[tuple[str, str]]:
    out: list[tuple[str, str]] = []
    pos = 0
    while pos < len(expr):
        m = TOKEN_RE.match(expr, pos)
        if not m:
            raise ValueError(f"unsupported predicate syntax near {expr[pos:]!r}")
        kind = m.lastgroup
        assert kind is not None
        out.append((kind, m.group(kind)))
        pos = m.end()
    return out


class Parser:
    def __init__(self, expr: str, command_ids: dict[str, int]):
        self.tokens = _tokenize(expr)
        self.i = 0
        self.command_ids = command_ids

    def _peek(self, value: str | None = None) -> bool:
        if self.i >= len(self.tokens):
            return False
        return value is None or self.tokens[self.i][1].lower() == value.lower()

    def _take(self, value: str | None = None) -> tuple[str, str]:
        if self.i >= len(self.tokens):
            raise ValueError("unexpected end of predicate")
        tok = self.tokens[self.i]
        if value is not None and tok[1].lower() != value.lower():
            raise ValueError(f"expected {value!r}, got {tok[1]!r}")
        self.i += 1
        return tok

    def parse(self) -> list[Insn]:
        code = self._or_expr()
        if self.i != len(self.tokens):
            raise ValueError(f"unexpected token {self.tokens[self.i][1]!r}")
        return code

    def _or_expr(self) -> list[Insn]:
        code = self._and_expr()
        while self._peek("||"):
            self._take("||")
            code += self._and_expr()
            code.append(Insn("or"))
        return code

    def _and_expr(self) -> list[Insn]:
        code = self._primary()
        while self._peek("&&"):
            self._take("&&")
            code += self._primary()
            code.append(Insn("and"))
        return code

    def _primary(self) -> list[Insn]:
        if self._peek("("):
            self._take("(")
            code = self._or_expr()
            self._take(")")
            return code

        kind, raw = self._take()
        if kind != "ident":
            raise ValueError(f"expected predicate identifier, got {raw!r}")
        name = raw.lower()
        if name == "ctrl":
            return [Insn("ctrl")]
        if name == "movecontact":
            return [Insn("move_contact")]

        _, cmpop = self._take()
        if cmpop not in ("=", "!=", ">", ">=", "<", "<="):
            raise ValueError(f"unsupported comparator {cmpop!r}")

        if name == "command":
            kind, value = self._take()
            if kind != "string":
                raise ValueError("command predicate requires a quoted name")
            cmd = value[1:-1]
            if cmd not in self.command_ids:
                raise ValueError(f"unknown command {cmd!r}")
            if cmpop == "=":
                return [Insn("command_active", self.command_ids[cmd])]
            if cmpop == "!=":
                return [Insn("command_inactive", self.command_ids[cmd])]
            raise ValueError("command supports only = and !=")

        if name == "statetype":
            kind, value = self._take()
            if kind != "ident" or value.upper() not in STATE_TYPE_VALUE:
                raise ValueError(f"unsupported statetype {value!r}")
            if cmpop == "=":
                return [Insn("state_type_eq", STATE_TYPE_VALUE[value.upper()])]
            if cmpop == "!=":
                return [Insn("state_type_ne", STATE_TYPE_VALUE[value.upper()])]
            raise ValueError("statetype supports only = and !=")

        if name == "stateno" and self._peek("["):
            if cmpop not in ("=", "!="):
                raise ValueError("state range supports only = and !=")
            self._take("[")
            k0, lo = self._take()
            self._take(",")
            k1, hi = self._take()
            self._take("]")
            if k0 != "number" or k1 != "number":
                raise ValueError("state range bounds must be integers")
            code = [Insn("state_no_range", int(lo), int(hi))]
            if cmpop == "!=":
                code.append(Insn("not"))
            return code

        kind, value = self._take()
        if kind != "number":
            raise ValueError(f"{name} comparison requires an integer")
        n = int(value)
        if name == "stateno":
            if cmpop == "=":
                return [Insn("state_no_eq", n)]
            if cmpop == "!=":
                return [Insn("state_no_ne", n)]
            raise ValueError("stateno scalar supports only = and !=")
        if name in ("time", "statetime"):
            opname = {
                "=": "state_time_eq", ">": "state_time_gt",
                ">=": "state_time_ge", "<": "state_time_lt",
                "<=": "state_time_le",
            }.get(cmpop)
            if opname is None:
                raise ValueError("time != is not supported")
            return [Insn(opname, n)]
        raise ValueError(f"unsupported predicate {name!r}")


def _and_join(parts: list[list[Insn]]) -> list[Insn]:
    if not parts:
        return []
    out = list(parts[0])
    for part in parts[1:]:
        out += part
        out.append(Insn("and"))
    return out


def _or_join(parts: list[list[Insn]]) -> list[Insn]:
    if not parts:
        return []
    out = list(parts[0])
    for part in parts[1:]:
        out += part
        out.append(Insn("or"))
    return out


def parse_state_rules(path: Path, targets: set[int]) -> tuple[list[Rule], list[str]]:
    cmd = parse_cmd(path)
    command_ids = {name: i for i, name in enumerate(cmd.names)}
    rules: list[Rule] = []
    diagnostics: list[str] = []

    for header, pairs in _state_sections(path):
        if not re.match(r"^state\s+-1(?:\s*,|$)", header, re.I):
            continue
        values = [v for k, v in pairs if k == "value"]
        types = [v for k, v in pairs if k == "type"]
        if not values or not types or types[-1].strip().lower() != "changestate":
            continue
        try:
            target = int(values[-1], 0)
        except ValueError:
            diagnostics.append(f"{header}: non-constant ChangeState value")
            continue
        if target not in targets:
            continue

        trigger_all = [v for k, v in pairs if k == "triggerall"]
        groups: dict[int, list[str]] = {}
        for key, value in pairs:
            m = re.fullmatch(r"trigger(\d+)", key)
            if m:
                groups.setdefault(int(m.group(1)), []).append(value)
        if not groups:
            raise ValueError(f"{header}: no trigger groups")

        try:
            all_code = _and_join([Parser(x, command_ids).parse() for x in trigger_all])
            group_code = _or_join([
                _and_join([Parser(x, command_ids).parse() for x in groups[n]])
                for n in sorted(groups)
            ])
            code = group_code if not all_code else all_code + group_code + [Insn("and")]
        except ValueError as exc:
            raise ValueError(f"{header}: {exc}") from exc

        if len(code) > 255:
            raise ValueError(f"{header}: predicate program exceeds 255 instructions")
        rules.append(Rule(header, target, code))

    missing = sorted(targets - {r.target for r in rules})
    if missing:
        raise ValueError(f"selected ChangeState targets not found: {missing}")
    return rules, diagnostics


def _ident(text: str) -> str:
    value = re.sub(r"[^A-Za-z0-9_]", "_", text)
    if not value or value[0].isdigit():
        value = "_" + value
    return value


def emit(rules: list[Rule], diagnostics: list[str], out_prefix: Path, symbol: str) -> None:
    symbol = _ident(symbol)
    instructions: list[Insn] = []
    rows: list[tuple[int, int, int]] = []
    for rule in rules:
        ofs = len(instructions)
        instructions.extend(rule.code)
        rows.append((ofs, len(rule.code), rule.target))

    insn_lines = [f"    {{{OP[i.op]}, 0u, {i.a}, {i.b}}}," for i in instructions]
    rule_lines = [f"    {{{ofs}u, {count}u, {target}, 0u}}," for ofs, count, target in rows]

    c = f"""/* Auto-generated by tools/ikemen_state_rules.py. */
#include "examples/ikemen_saturn/ikemen_command.h"
#include "{out_prefix.name}.h"

static const ik_state_rule_instr_t {symbol}_state_rule_code[{max(1, len(instructions))}] = {{
{chr(10).join(insn_lines) if insn_lines else '    {0u, 0u, 0, 0},'}
}};

static const ik_state_rule_t {symbol}_state_rule_rows[{max(1, len(rules))}] = {{
{chr(10).join(rule_lines) if rule_lines else '    {0u, 0u, 0, 0u},'}
}};

const ik_state_rule_asset_t {symbol}_state_rules = {{
    {symbol}_state_rule_code, {len(instructions)}u,
    {symbol}_state_rule_rows, {len(rules)}u
}};
"""
    h = f"""/* Auto-generated by tools/ikemen_state_rules.py. */
#pragma once

#include "examples/ikemen_saturn/ikemen_command.h"

#define {symbol.upper()}_STATE_RULE_COUNT {len(rules)}u
#define {symbol.upper()}_STATE_RULE_INSTRUCTION_COUNT {len(instructions)}u

extern const ik_state_rule_asset_t {symbol}_state_rules;
"""
    out_prefix.parent.mkdir(parents=True, exist_ok=True)
    out_prefix.with_suffix(".c").write_text(c, encoding="utf-8")
    out_prefix.with_suffix(".h").write_text(h, encoding="utf-8")
    out_prefix.with_suffix(".json").write_text(json.dumps({
        "rule_count": len(rules),
        "instruction_count": len(instructions),
        "targets": [r.target for r in rules],
        "rules": [{
            "label": r.label,
            "target": r.target,
            "instructions": [{"op": i.op, "a": i.a, "b": i.b} for i in r.code],
        } for r in rules],
        "diagnostics": diagnostics,
    }, indent=2), encoding="utf-8")


def main(argv: list[str] | None = None) -> int:
    p = argparse.ArgumentParser()
    p.add_argument("--cmd", required=True, type=Path)
    p.add_argument("--targets", required=True)
    p.add_argument("--out-prefix", required=True, type=Path)
    p.add_argument("--symbol", required=True)
    args = p.parse_args(argv)
    targets = {int(v.strip(), 0) for v in args.targets.split(",") if v.strip()}
    rules, diagnostics = parse_state_rules(args.cmd, targets)
    emit(rules, diagnostics, args.out_prefix, args.symbol)
    print(f"[ikemen_state_rules] {args.symbol}: rules={len(rules)} targets={len(targets)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
