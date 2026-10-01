#!/usr/bin/env python3
from __future__ import annotations

from pathlib import Path

BUTTON_BITS = {
    "forward": 1 << 0,
    "back": 1 << 1,
    "up": 1 << 2,
    "down": 1 << 3,
    "a": 1 << 4,
    "b": 1 << 5,
    "c": 1 << 6,
    "x": 1 << 7,
    "y": 1 << 8,
    "z": 1 << 9,
    "start": 1 << 10,
}

def _mask(buttons: list[str]) -> int:
    mask = 0
    for button in buttons:
        key = str(button).lower()
        if key not in BUTTON_BITS:
            raise ValueError(f"unsupported oracle input button: {button!r}")
        mask |= BUTTON_BITS[key]
    return mask

def build_timeline(scenario: dict) -> list[tuple[int, int]]:
    frames = int(scenario["frames"])
    timeline = [(0, 0) for _ in range(frames)]
    for event in scenario.get("inputs", []):
        start = int(event.get("from", event.get("frame", 0)))
        end = int(event.get("to", start))
        if start < 0 or end < start or end >= frames:
            raise ValueError(
                f"invalid oracle input range {start}..{end} "
                f"for {frames} frames"
            )
        p1 = _mask(list(event.get("p1", [])))
        p2 = _mask(list(event.get("p2", [])))
        for frame in range(start, end + 1):
            timeline[frame] = (p1, p2)
    return timeline

def write_timeline(path: Path, scenario: dict) -> None:
    timeline = build_timeline(scenario)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(
        "".join(f"{p1} {p2}\n" for p1, p2 in timeline),
        encoding="utf-8",
    )
