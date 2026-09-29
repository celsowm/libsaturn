"""AIR animation parsing: actions, frames, times, flips, CLSN boxes.

Superset of the minimal parser in tools/ikemen_char_to_saturn.py (which
only needed frame counts for the metadata manifest). This module is the
canonical parser for the runtime asset pipeline: it keeps per-frame
axis offsets, durations, flips and collision boxes.

AIR frame syntax handled:
    [Begin Action N]
    Clsn2Default: 2
    Clsn2[0] = l,t,r,b          (default boxes, action-wide)
    Clsn2: 1
    Clsn2[0] = l,t,r,b          (per-frame boxes, apply to next frame)
    group,number, x,y, time [, H|V|HV] [, trans...]
"""
from __future__ import annotations

from dataclasses import dataclass, field
from pathlib import Path
import re

_BEGIN = re.compile(r"\[Begin Action\s+(-?\d+)\]", re.I)
_FRAME = re.compile(r"^\s*(-?\d+)\s*,\s*(-?\d+)\s*,\s*(-?\d+)\s*,\s*(-?\d+)\s*,\s*(-?\d+)(.*)$")
_BOX = re.compile(r"^\s*Clsn2\s*\[\s*(\d+)\s*\]\s*=\s*(-?\d+)\s*,\s*(-?\d+)\s*,\s*(-?\d+)\s*,\s*(-?\d+)", re.I)
_CLSN_DEFAULT_COUNT = re.compile(r"^\s*Clsn2Default\s*:\s*(\d+)", re.I)
_CLSN_COUNT = re.compile(r"^\s*Clsn2\s*:\s*(\d+)", re.I)


@dataclass
class AirFrame:
    group: int
    number: int
    x: int
    y: int
    time: int
    flip_h: bool = False
    flip_v: bool = False
    clsn2: list[tuple[int, int, int, int]] = field(default_factory=list)


@dataclass
class AirAction:
    number: int
    frames: list[AirFrame] = field(default_factory=list)
    clsn2_default: list[tuple[int, int, int, int]] = field(default_factory=list)


def _strip_comment(line: str) -> str:
    # ';' starts a comment; an escaped '\;' is a literal semicolon.
    out = []
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
    pending_clsn: list[tuple[int, int, int, int]] = []
    collecting_default = 0     # remaining default-box lines to consume
    collecting_frame_clsn = 0  # remaining per-frame box lines to consume

    def consume_box(line: str, boxes: list[tuple[int, int, int, int]]) -> bool:
        m = _BOX.match(line)
        if m:
            boxes.append((int(m.group(2)), int(m.group(3)),
                         int(m.group(4)), int(m.group(5))))
            return True
        return False

    for raw in Path(path).read_text(encoding="utf-8", errors="ignore").splitlines():
        line = _strip_comment(raw).strip()
        if not line:
            continue
        m = _BEGIN.match(line)
        if m:
            current = AirAction(int(m.group(1)))
            actions[current.number] = current
            pending_clsn = []
            collecting_default = collecting_frame_clsn = 0
            continue
        if current is None:
            continue
        m = _CLSN_DEFAULT_COUNT.match(line)
        if m:
            collecting_default = int(m.group(1))
            collecting_frame_clsn = 0
            continue
        m = _CLSN_COUNT.match(line)
        if m:
            collecting_frame_clsn = int(m.group(1))
            collecting_default = 0
            continue
        if collecting_default > 0 and consume_box(line, current.clsn2_default):
            collecting_default -= 1
            continue
        if collecting_frame_clsn > 0 and consume_box(line, pending_clsn):
            collecting_frame_clsn -= 1
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
            frame = AirFrame(int(m.group(1)), int(m.group(2)),
                             int(m.group(3)), int(m.group(4)),
                             int(m.group(5)), flip_h, flip_v, pending_clsn)
            pending_clsn = []
            current.frames.append(frame)
    return actions


def find(actions: dict[int, AirAction], number: int) -> AirAction | None:
    return actions.get(number)
