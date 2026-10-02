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
MOVE_TYPE_VALUE = {"I": 1, "A": 2, "H": 3}
ATTACK_ATTR_VALUE = {
    "NA": 1 << 0,
    "SA": 1 << 1,
    "HA": 1 << 2,
    "NP": 1 << 3,
    "SP": 1 << 4,
    "HP": 1 << 5,
    "NT": 1 << 6,
    "ST": 1 << 7,
    "HT": 1 << 8,
}


@dataclass(frozen=True)
class Insn:
    op: str
    a: int = 0
    b: int = 0
    c: int = 0


@dataclass
class Rule:
    label: str
    target: int
    code: list[Insn]


@dataclass(frozen=True)
class VmInsn:
    op: str
    field: str = "0u"
    redirect: str = "IK_EXPR_REDIRECT_SELF"
    a: int = 0
    b: int = 0
    reserved: int = 0


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
    def __init__(
        self,
        expr: str,
        command_ids: dict[str, int],
        variables: dict[int, list[Insn]] | None = None,
    ):
        self.tokens = _tokenize(expr)
        self.i = 0
        self.command_ids = command_ids
        self.variables = variables or {}

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

        if name == "var":
            self._take("(")
            kind, value = self._take()
            self._take(")")
            if kind != "number":
                raise ValueError("var() index must be an integer")
            index = int(value)
            if index not in self.variables:
                raise ValueError(f"unsupported variable predicate var({index})")
            return list(self.variables[index])

        if name == "p2bodydist":
            axis_kind, axis = self._take()
            if axis_kind != "ident" or axis.lower() != "x":
                raise ValueError("only p2bodydist X is supported")
            _, cmpop = self._take()
            if cmpop != "<":
                raise ValueError("p2bodydist X currently supports only <")
            kind, value = self._take()
            if kind != "number":
                raise ValueError("p2bodydist X comparison requires an integer")
            return [Insn("p2_body_dist_x_lt", int(value))]

        if name == "target":
            target_id = -1
            target_index = 0
            if self._peek("("):
                self._take("(")
                id_kind, id_value = self._take()
                if id_kind != "number":
                    raise ValueError("target() ID must be an integer")
                target_id = int(id_value)
                if self._peek(","):
                    self._take(",")
                    index_kind, index_value = self._take()
                    if index_kind != "number":
                        raise ValueError("target() index must be an integer")
                    target_index = int(index_value)
                    if target_index < 0 or target_index > 255:
                        raise ValueError("target() index must be in 0..255")
                self._take(")")
            self._take(",")
            field_kind, field_name = self._take()
            if field_kind != "ident":
                raise ValueError("target redirect requires a field")
            field_name = field_name.lower()
            _, cmpop = self._take()
            if cmpop not in ("=", "!=", ">", ">=", "<", "<="):
                raise ValueError(
                    f"unsupported target redirect comparator {cmpop!r}"
                )

            field_map = {
                "stateno": "state_no",
                "statetype": "state_type",
                "movetype": "move_type",
                "life": "life",
                "ctrl": "ctrl",
            }
            if field_name not in field_map:
                raise ValueError(
                    f"unsupported target redirect field {field_name!r}"
                )

            value_kind, value = self._take()
            if field_name == "statetype":
                if value_kind != "ident" or value.upper() not in STATE_TYPE_VALUE:
                    raise ValueError(f"unsupported target statetype {value!r}")
                rhs = STATE_TYPE_VALUE[value.upper()]
            elif field_name == "movetype":
                if value_kind != "ident" or value.upper() not in MOVE_TYPE_VALUE:
                    raise ValueError(f"unsupported target movetype {value!r}")
                rhs = MOVE_TYPE_VALUE[value.upper()]
            else:
                if value_kind != "number":
                    raise ValueError(
                        f"target {field_name} comparison requires an integer"
                    )
                rhs = int(value)

            suffix = {
                "=": "eq", "!=": "ne", ">": "gt", ">=": "ge",
                "<": "lt", "<=": "le",
            }[cmpop]
            return [
                Insn(
                    f"target_{field_map[field_name]}_{suffix}",
                    target_id,
                    rhs,
                    target_index,
                )
            ]

        if name == "numtarget":
            target_id = -1
            if self._peek("("):
                self._take("(")
                id_kind, id_value = self._take()
                self._take(")")
                if id_kind != "number":
                    raise ValueError("NumTarget() id must be an integer")
                target_id = int(id_value)
            _, cmpop = self._take()
            if cmpop not in ("=", "!=", ">", ">=", "<", "<="):
                raise ValueError(f"unsupported NumTarget comparator {cmpop!r}")
            value_kind, value = self._take()
            if value_kind != "number":
                raise ValueError("NumTarget comparison requires an integer")
            suffix = {
                "=": "eq", "!=": "ne", ">": "gt", ">=": "ge",
                "<": "lt", "<=": "le",
            }[cmpop]
            return [Insn(f"num_targets_{suffix}", target_id, int(value))]

        _, cmpop = self._take()
        if cmpop not in ("=", "!=", ">", ">=", "<", "<="):
            raise ValueError(f"unsupported comparator {cmpop!r}")

        if name == "hitdefattr":
            if cmpop != "=":
                raise ValueError("hitdefattr currently supports only =")
            kinds: list[str] = []
            first_kind, first_value = self._take()
            if first_kind != "ident":
                raise ValueError("hitdefattr state class must be an identifier")
            state_class = first_value.upper()
            if not state_class or any(ch not in "SCA" for ch in state_class):
                raise ValueError(
                    f"unsupported hitdefattr state class {first_value!r}"
                )
            while self._peek(","):
                self._take(",")
                kind, value = self._take()
                if kind != "ident" or value.upper() not in ATTACK_ATTR_VALUE:
                    raise ValueError(
                        f"unsupported hitdefattr attack attr {value!r}"
                    )
                kinds.append(value.upper())
            if not kinds:
                raise ValueError("hitdefattr requires attack attributes")
            parts = [
                [Insn("active_hit_attr_eq", ATTACK_ATTR_VALUE[value])]
                for value in kinds
            ]
            return _or_join(parts)

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

        if name == "p2statetype":
            kind, value = self._take()
            if kind != "ident" or value.upper() not in STATE_TYPE_VALUE:
                raise ValueError(f"unsupported p2statetype {value!r}")
            if cmpop == "=":
                return [Insn("p2_state_type_eq", STATE_TYPE_VALUE[value.upper()])]
            if cmpop == "!=":
                return [Insn("p2_state_type_ne", STATE_TYPE_VALUE[value.upper()])]
            raise ValueError("p2statetype supports only = and !=")

        if name == "p2movetype":
            kind, value = self._take()
            if kind != "ident" or value.upper() not in MOVE_TYPE_VALUE:
                raise ValueError(f"unsupported p2movetype {value!r}")
            if cmpop == "=":
                return [Insn("p2_move_type_eq", MOVE_TYPE_VALUE[value.upper()])]
            if cmpop == "!=":
                return [Insn("p2_move_type_ne", MOVE_TYPE_VALUE[value.upper()])]
            raise ValueError("p2movetype supports only = and !=")

        if name == "stateno" and (self._peek("[") or self._peek("(")):
            if cmpop not in ("=", "!="):
                raise ValueError("state range supports only = and !=")
            # Mugen intervals may be half-open: [a,b) (a,b] (a,b).
            open_exclusive = self._take()[1] == "("
            k0, lo = self._take()
            self._take(",")
            k1, hi = self._take()
            close = self._take()[1]
            if close not in ("]", ")"):
                raise ValueError(f"expected ']' or ')', got {close!r}")
            if k0 != "number" or k1 != "number":
                raise ValueError("state range bounds must be integers")
            lo_n = int(lo) + (1 if open_exclusive else 0)
            hi_n = int(hi) - (1 if close == ")" else 0)
            code = [Insn("state_no_range", lo_n, hi_n)]
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
        if name == "power":
            opname = {
                "=": "power_eq", ">": "power_gt", ">=": "power_ge",
                "<": "power_lt", "<=": "power_le",
            }.get(cmpop)
            if opname is None:
                raise ValueError("power != is not supported")
            return [Insn(opname, n)]

        projectile_queries = {
            "numproj": "num_projectiles",
            "projcontact": "proj_contact",
            "projhit": "proj_hit",
            "projguarded": "proj_guarded",
            "projcontacttime": "proj_contact_time",
            "projhittime": "proj_hit_time",
            "projguardedtime": "proj_guarded_time",
        }
        if name in projectile_queries:
            suffix = {
                "=": "eq", "!=": "ne", ">": "gt", ">=": "ge",
                "<": "lt", "<=": "le",
            }[cmpop]
            return [Insn(f"{projectile_queries[name]}_{suffix}", n)]

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


def _predicate_code(
    pairs: list[tuple[str, str]],
    command_ids: dict[str, int],
    variables: dict[int, list[Insn]],
) -> list[Insn]:
    trigger_all = [v for k, v in pairs if k == "triggerall"]
    groups: dict[int, list[str]] = {}
    for key, value in pairs:
        m = re.fullmatch(r"trigger(\d+)", key)
        if m:
            groups.setdefault(int(m.group(1)), []).append(value)
    if not groups:
        raise ValueError("no trigger groups")

    all_code = _and_join([
        Parser(x, command_ids, variables).parse() for x in trigger_all
    ])
    group_code = _or_join([
        _and_join([
            Parser(x, command_ids, variables).parse() for x in groups[n]
        ])
        for n in sorted(groups)
    ])
    return (
        group_code
        if not all_code
        else all_code + group_code + [Insn("and")]
    )


def parse_state_rules(path: Path, targets: set[int]) -> tuple[list[Rule], list[str]]:
    cmd = parse_cmd(path)
    command_ids = {name: i for i, name in enumerate(cmd.names)}
    sections = _state_sections(path)
    rules: list[Rule] = []
    diagnostics: list[str] = []
    variables: dict[int, list[Insn]] = {}

    # Inline derived State -1 variables such as KFM's var(1) combo gate.
    # A VarSet to 1 is a pure boolean predicate here; the matching reset to
    # zero is implicit because the generated ChangeState rule re-evaluates it
    # every frame instead of storing mutable CMD variables at runtime.
    for header, pairs in sections:
        if not re.match(r"^state\s+-1(?:\s*,|$)", header, re.I):
            continue
        types = [v for k, v in pairs if k == "type"]
        if not types or types[-1].strip().lower() != "varset":
            continue
        assignment = next(
            (
                (int(m.group(1)), v)
                for k, v in pairs
                for m in [re.fullmatch(r"var\((\d+)\)", k)]
                if m
            ),
            None,
        )
        if assignment is None:
            continue
        index, raw_value = assignment
        try:
            enabled = int(raw_value, 0)
        except ValueError:
            continue
        if enabled != 1:
            continue
        try:
            variables[index] = _predicate_code(
                pairs, command_ids, variables
            )
        except ValueError as exc:
            raise ValueError(f"{header}: {exc}") from exc

    for header, pairs in sections:
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

        try:
            code = _predicate_code(pairs, command_ids, variables)
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


def _vm_compare(
    field: str,
    redirect: str,
    compare_op: str,
    value: int,
) -> list[VmInsn]:
    return [
        VmInsn("IK_EXPR_LOAD_FIELD", field, redirect),
        VmInsn("IK_EXPR_PUSH_CONST", a=value),
        VmInsn(compare_op),
    ]


def _lower_instruction(insn: Insn) -> list[VmInsn]:
    self_r = "IK_EXPR_REDIRECT_SELF"
    p2_r = "IK_EXPR_REDIRECT_P2"

    if insn.op == "command_active":
        return [VmInsn("IK_EXPR_LOAD_COMMAND", a=insn.a)]
    if insn.op == "command_inactive":
        return [
            VmInsn("IK_EXPR_LOAD_COMMAND", a=insn.a),
            VmInsn("IK_EXPR_NOT"),
        ]

    simple = {
        "state_type_eq": ("IK_EXPR_FIELD_STATE_TYPE", self_r, "IK_EXPR_EQ"),
        "state_type_ne": ("IK_EXPR_FIELD_STATE_TYPE", self_r, "IK_EXPR_NE"),
        "state_no_eq": ("IK_EXPR_FIELD_STATE_NO", self_r, "IK_EXPR_EQ"),
        "state_no_ne": ("IK_EXPR_FIELD_STATE_NO", self_r, "IK_EXPR_NE"),
        "state_time_eq": ("IK_EXPR_FIELD_STATE_TIME", self_r, "IK_EXPR_EQ"),
        "state_time_gt": ("IK_EXPR_FIELD_STATE_TIME", self_r, "IK_EXPR_GT"),
        "state_time_ge": ("IK_EXPR_FIELD_STATE_TIME", self_r, "IK_EXPR_GE"),
        "state_time_lt": ("IK_EXPR_FIELD_STATE_TIME", self_r, "IK_EXPR_LT"),
        "state_time_le": ("IK_EXPR_FIELD_STATE_TIME", self_r, "IK_EXPR_LE"),
        "p2_body_dist_x_lt": ("IK_EXPR_FIELD_BODY_DIST_X", p2_r, "IK_EXPR_LT"),
        "p2_state_type_eq": ("IK_EXPR_FIELD_STATE_TYPE", p2_r, "IK_EXPR_EQ"),
        "p2_state_type_ne": ("IK_EXPR_FIELD_STATE_TYPE", p2_r, "IK_EXPR_NE"),
        "p2_move_type_eq": ("IK_EXPR_FIELD_MOVE_TYPE", p2_r, "IK_EXPR_EQ"),
        "p2_move_type_ne": ("IK_EXPR_FIELD_MOVE_TYPE", p2_r, "IK_EXPR_NE"),
        "power_eq": ("IK_EXPR_FIELD_POWER", self_r, "IK_EXPR_EQ"),
        "power_gt": ("IK_EXPR_FIELD_POWER", self_r, "IK_EXPR_GT"),
        "power_ge": ("IK_EXPR_FIELD_POWER", self_r, "IK_EXPR_GE"),
        "power_lt": ("IK_EXPR_FIELD_POWER", self_r, "IK_EXPR_LT"),
        "power_le": ("IK_EXPR_FIELD_POWER", self_r, "IK_EXPR_LE"),
        "active_hit_attr_eq": (
            "IK_EXPR_FIELD_ACTIVE_HIT_ATTR", self_r, "IK_EXPR_EQ"),
    }
    query_fields = {
        "num_projectiles": "IK_EXPR_FIELD_NUM_PROJECTILES",
        "proj_contact": "IK_EXPR_FIELD_PROJ_CONTACT",
        "proj_hit": "IK_EXPR_FIELD_PROJ_HIT",
        "proj_guarded": "IK_EXPR_FIELD_PROJ_GUARDED",
        "proj_contact_time": "IK_EXPR_FIELD_PROJ_CONTACT_TIME",
        "proj_hit_time": "IK_EXPR_FIELD_PROJ_HIT_TIME",
        "proj_guarded_time": "IK_EXPR_FIELD_PROJ_GUARDED_TIME",
    }
    for prefix, field in query_fields.items():
        for suffix, compare in (
            ("eq", "IK_EXPR_EQ"), ("ne", "IK_EXPR_NE"),
            ("gt", "IK_EXPR_GT"), ("ge", "IK_EXPR_GE"),
            ("lt", "IK_EXPR_LT"), ("le", "IK_EXPR_LE"),
        ):
            simple[f"{prefix}_{suffix}"] = (field, self_r, compare)
    if insn.op in simple:
        field, redirect, compare_op = simple[insn.op]
        return _vm_compare(field, redirect, compare_op, insn.a)

    if insn.op.startswith("target_"):
        match = re.fullmatch(
            r"target_(state_no|state_type|move_type|life|ctrl)_"
            r"(eq|ne|gt|ge|lt|le)",
            insn.op,
        )
        if not match:
            raise ValueError(f"bad target redirect opcode {insn.op!r}")
        field = {
            "state_no": "IK_EXPR_FIELD_STATE_NO",
            "state_type": "IK_EXPR_FIELD_STATE_TYPE",
            "move_type": "IK_EXPR_FIELD_MOVE_TYPE",
            "life": "IK_EXPR_FIELD_LIFE",
            "ctrl": "IK_EXPR_FIELD_CTRL",
        }[match.group(1)]
        compare = {
            "eq": "IK_EXPR_EQ",
            "ne": "IK_EXPR_NE",
            "gt": "IK_EXPR_GT",
            "ge": "IK_EXPR_GE",
            "lt": "IK_EXPR_LT",
            "le": "IK_EXPR_LE",
        }[match.group(2)]
        return [
            VmInsn(
                "IK_EXPR_LOAD_FIELD",
                field,
                "IK_EXPR_REDIRECT_TARGET",
                0,
                insn.a,
                insn.c,
            ),
            VmInsn("IK_EXPR_PUSH_CONST", a=insn.b),
            VmInsn(compare),
        ]

    if insn.op.startswith("num_targets_"):
        compare = {
            "num_targets_eq": "IK_EXPR_EQ",
            "num_targets_ne": "IK_EXPR_NE",
            "num_targets_gt": "IK_EXPR_GT",
            "num_targets_ge": "IK_EXPR_GE",
            "num_targets_lt": "IK_EXPR_LT",
            "num_targets_le": "IK_EXPR_LE",
        }.get(insn.op)
        if compare is None:
            raise ValueError(f"bad NumTarget opcode {insn.op!r}")
        return [
            VmInsn(
                "IK_EXPR_LOAD_FIELD",
                "IK_EXPR_FIELD_NUM_TARGETS",
                self_r,
                insn.a,
                -1,
            ),
            VmInsn("IK_EXPR_PUSH_CONST", a=insn.b),
            VmInsn(compare),
        ]

    if insn.op == "state_no_range":
        return (
            _vm_compare(
                "IK_EXPR_FIELD_STATE_NO", self_r, "IK_EXPR_GE", insn.a)
            + _vm_compare(
                "IK_EXPR_FIELD_STATE_NO", self_r, "IK_EXPR_LE", insn.b)
            + [VmInsn("IK_EXPR_AND")]
        )

    if insn.op == "ctrl":
        return [VmInsn(
            "IK_EXPR_LOAD_FIELD", "IK_EXPR_FIELD_CTRL", self_r)]
    if insn.op == "move_contact":
        return [VmInsn(
            "IK_EXPR_LOAD_FIELD", "IK_EXPR_FIELD_MOVE_CONTACT", self_r)]

    logic = {
        "not": "IK_EXPR_NOT",
        "and": "IK_EXPR_AND",
        "or": "IK_EXPR_OR",
    }
    if insn.op in logic:
        return [VmInsn(logic[insn.op])]

    raise ValueError(f"cannot lower state-rule opcode {insn.op!r}")


def required_command(code: list[Insn]) -> int | None:
    """A command id the rule cannot pass without, or None.

    Evaluates the postfix program over sets of commands that must be active.
    AND unions its operands' requirements, OR keeps only what both sides
    need, and anything that is not a positive command test (including NOT and
    command_inactive) requires nothing. The result is a necessary condition,
    so a runtime may skip the rule whenever that command is inactive and get
    the same answer without interpreting it. Returns None for a program this
    cannot analyse, which is always safe.
    """
    stack: list[frozenset[int]] = []
    for insn in code:
        if insn.op == "command_active":
            stack.append(frozenset({insn.a}))
        elif insn.op == "not":
            if not stack:
                return None
            stack.pop()
            stack.append(frozenset())
        elif insn.op in ("and", "or"):
            if len(stack) < 2:
                return None
            right = stack.pop()
            left = stack.pop()
            stack.append(left | right if insn.op == "and" else left & right)
        else:
            stack.append(frozenset())
    if len(stack) != 1 or not stack[0]:
        return None
    return min(stack[0])


def emit(rules: list[Rule], diagnostics: list[str], out_prefix: Path, symbol: str) -> None:
    symbol = _ident(symbol)
    instructions: list[VmInsn] = []
    rows: list[tuple[int, int, int, int]] = []

    for rule in rules:
        lowered: list[VmInsn] = []
        for insn in rule.code:
            lowered.extend(_lower_instruction(insn))
        if len(lowered) > 255:
            raise ValueError(
                f"{rule.label}: lowered predicate exceeds 255 instructions")
        ofs = len(instructions)
        instructions.extend(lowered)
        # command_gate: 0 = always evaluate, else (required command id) + 1.
        required = required_command(rule.code)
        gate = 0 if required is None else required + 1
        if gate > 255:
            gate = 0
        rows.append((ofs, len(lowered), rule.target, gate))

    insn_lines = [
        f"    {{{i.op}, {i.field}, {i.redirect}, {i.reserved}u, {i.a}, {i.b}}},"
        for i in instructions
    ]
    rule_lines = [
        f"    {{{ofs}u, {count}u, {target}, {gate}u}},"
        for ofs, count, target, gate in rows
    ]

    c = f"""/* Auto-generated by tools/ikemen_state_rules.py. */
#include "examples/ikemen_saturn/ikemen_command.h"
#include "{out_prefix.name}.h"

static const ik_state_rule_instr_t {symbol}_state_rule_code[{max(1, len(instructions))}] = {{
{chr(10).join(insn_lines) if insn_lines else '    {0u, 0u, 0u, 0u, 0, 0},'}
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
        "source_instruction_count": sum(len(r.code) for r in rules),
        "targets": [r.target for r in rules],
        "rules": [{
            "label": r.label,
            "target": r.target,
            "required_command": required_command(r.code),
            "source_instructions": [
                {"op": i.op, "a": i.a, "b": i.b, "c": i.c}
                for i in r.code
            ],
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
