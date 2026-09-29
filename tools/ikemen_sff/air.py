"""AIR animation parsing: actions, frames, times, flips, Clsn1/Clsn2 boxes.

The runtime asset pipeline resolves AIR collision defaults at parse time, so
every AirFrame owns the exact collision boxes that apply to that element.
That matters for MUGEN/Ikemen semantics: "Clsn1: 0" explicitly clears an
action default for the next frame and a later Clsn*Default may replace an
earlier default mid-action.
"""
from __future__ import annotations

from dataclasses import dataclass, field
from pathlib import Path
import re

_BEGIN = re.compile(r"\[Begin Action\s+(-?\d+)\]", re.I)
_FRAME = re.compile(
    r"^\s*(-?\d+)\s*,\s*(-?\d+)\s*,\s*(-?\d+)\s*,\s*(-?\d+)\s*,\s*(-?\d+)(.*)$"
)
_BOX = re.compile(
    r"^\s*Clsn([12])\s*\[\s*(\d+)\s*\]\s*=\s*"
    r"(-?\d+)\s*,\s*(-?\d+)\s*,\s*(-?\d+)\s*,\s*(-?\d+)",
    re.I,
)
_CLSN_DEFAULT_COUNT = re.compile(r"^\s*Clsn([12])Default\s*:\s*(\d+)", re.I)
_CLSN_COUNT = re.compile(r"^\s*Clsn([12])\s*:\s*(\d+)", re.I)

Box = tuple[int, int, int, int]


@dataclass
class AirFrame:
    group: int
    number: int
    x: int
    y: int
    time: int
    flip_h: bool = False
    flip_v: bool = False
    clsn1: list[Box] = field(default_factory=list)
    clsn2: list[Box] = field(default_factory=list)


@dataclass
class AirAction:
    number: int
    frames: list[AirFrame] = field(default_factory=list)
    clsn1_default: list[Box] = field(default_factory=list)
    clsn2_default: list[Box] = field(default_factory=list)


def _strip_comment(line: str) -> str:
    out: list[str] = []
    escaped = False
    for ch in line:
        if escaped:
            out.append(ch)
            escaped = False
            continue
        if ch == "\\":
            escaped = True
            continue
        if ch == ";":
            break
        out.append(ch)
    return "".join(out)


def parse(path: Path) -> dict[int, AirAction]:
    actions: dict[int, AirAction] = {}
    current: AirAction | None = None
    pending: dict[int, list[Box] | None] = {1: None, 2: None}
    collecting_kind = 0
    collecting_default = False
    collecting_remaining = 0

    def defaults(kind: int) -> list[Box]:
        assert current is not None
        return current.clsn1_default if kind == 1 else current.clsn2_default

    def set_defaults(kind: int, value: list[Box]) -> None:
        assert current is not None
        if kind == 1:
            current.clsn1_default = value
        else:
            current.clsn2_default = value

    for raw in Path(path).read_text(encoding="utf-8", errors="ignore").splitlines():
        line = _strip_comment(raw).strip()
        if not line:
            continue

        m = _BEGIN.match(line)
        if m:
            current = AirAction(int(m.group(1)))
            actions[current.number] = current
            pending = {1: None, 2: None}
            collecting_kind = 0
            collecting_default = False
            collecting_remaining = 0
            continue
        if current is None:
            continue

        m = _CLSN_DEFAULT_COUNT.match(line)
        if m:
            kind = int(m.group(1))
            set_defaults(kind, [])
            collecting_kind = kind
            collecting_default = True
            collecting_remaining = int(m.group(2))
            continue

        m = _CLSN_COUNT.match(line)
        if m:
            kind = int(m.group(1))
            pending[kind] = []
            collecting_kind = kind
            collecting_default = False
            collecting_remaining = int(m.group(2))
            continue

        m = _BOX.match(line)
        if m and collecting_remaining > 0 and int(m.group(1)) == collecting_kind:
            box: Box = (
                int(m.group(3)), int(m.group(4)),
                int(m.group(5)), int(m.group(6)),
            )
            if collecting_default:
                defaults(collecting_kind).append(box)
            else:
                assert pending[collecting_kind] is not None
                pending[collecting_kind].append(box)
            collecting_remaining -= 1
            continue

        m = _FRAME.match(line)
        if m:
            rest = m.group(6)
            flip_h = flip_v = False
            for token in rest.split(","):
                token = token.strip().upper()
                if token in ("H", "HV"):
                    flip_h = True
                if token in ("V", "HV"):
                    flip_v = True

            effective1 = (
                list(current.clsn1_default)
                if pending[1] is None else list(pending[1])
            )
            effective2 = (
                list(current.clsn2_default)
                if pending[2] is None else list(pending[2])
            )
            current.frames.append(AirFrame(
                int(m.group(1)), int(m.group(2)),
                int(m.group(3)), int(m.group(4)), int(m.group(5)),
                flip_h, flip_v, effective1, effective2,
            ))
            pending = {1: None, 2: None}
            collecting_kind = 0
            collecting_default = False
            collecting_remaining = 0

    return actions


def find(actions: dict[int, AirAction], number: int) -> AirAction | None:
    return actions.get(number)
