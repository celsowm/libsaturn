"""Animation clip packing for saturn/sprite_clip.h.

A spec lists clips of frames; a frame names its source rectangle (explicitly or as a cell of a
sheet grid), pivot, duration in ticks, an event id and generic shapes. Identical shape lists are
stored once and shared, even as part of a longer run, by the frames that use them.
"""

from __future__ import annotations

from dataclasses import dataclass, field

from .errors import Stage2dError

MODES = {"once": 0, "loop": 1, "ping_pong": 2}


@dataclass
class Shape:
    x: int
    y: int
    w: int
    h: int
    kind: int
    index: int
    flags: int


@dataclass
class Frame:
    source: tuple[int, int, int, int]  # x, y, w, h
    pivot: tuple[int, int]
    duration: int
    event: int
    shape_first: int
    shape_count: int
    flags: int


@dataclass
class Clip:
    name: str
    mode: int
    loop_start: int
    frames: list[Frame]


@dataclass
class ClipSet:
    clips: list[Clip] = field(default_factory=list)
    shapes: list[Shape] = field(default_factory=list)

    @property
    def region_count(self) -> int:
        return sum(len(c.frames) for c in self.clips)


def _int(value, what: str, lo: int, hi: int) -> int:
    if isinstance(value, bool) or not isinstance(value, int) or not lo <= value <= hi:
        raise Stage2dError(f"{what}: {value!r} is outside {lo}..{hi}")
    return value


def _source(frame: dict, sheet, what: str) -> tuple[int, int, int, int]:
    if "source" in frame:
        s = frame["source"]
        if not isinstance(s, list) or len(s) != 4:
            raise Stage2dError(f"{what}: source is [x, y, w, h]")
        x, y = _int(s[0], what + " source x", -32768, 32767), _int(s[1], what + " source y", -32768, 32767)
        w, h = _int(s[2], what + " source w", 1, 65535), _int(s[3], what + " source h", 1, 65535)
        return (x, y, w, h)
    if "cell" in frame:
        if not sheet:
            raise Stage2dError(f"{what}: 'cell' needs a top-level 'sheet'")
        cell = _int(frame["cell"], what + " cell", 0, 0xFFFF)
        cw, ch, cols = sheet["cell_w"], sheet["cell_h"], sheet["columns"]
        span = frame.get("span", [1, 1])
        x = sheet.get("origin_x", 0) + (cell % cols) * cw
        y = sheet.get("origin_y", 0) + (cell // cols) * ch
        return (_int(x, what + " cell x", -32768, 32767), _int(y, what + " cell y", -32768, 32767),
                _int(cw * span[0], what + " cell w", 1, 65535), _int(ch * span[1], what + " cell h", 1, 65535))
    raise Stage2dError(f"{what}: needs source or cell")


def _pivot(frame: dict, source, what: str) -> tuple[int, int]:
    p = frame.get("pivot", "bottom_center")
    w, h = source[2], source[3]
    named = {"top_left": (0, 0), "center": (w // 2, h // 2), "bottom_center": (w // 2, h),
             "bottom_left": (0, h), "top_center": (w // 2, 0)}
    if isinstance(p, str):
        if p not in named:
            raise Stage2dError(f"{what}: unknown pivot '{p}' (use one of {', '.join(named)} or [x, y])")
        return named[p]
    if not isinstance(p, list) or len(p) != 2:
        raise Stage2dError(f"{what}: pivot is [x, y] or a name")
    return (_int(p[0], what + " pivot x", -32768, 32767), _int(p[1], what + " pivot y", -32768, 32767))


def _find_run(haystack: list[tuple], needle: list[tuple]) -> int:
    """Index where `needle` already appears as a contiguous run of `haystack`, or -1."""
    n = len(needle)
    for i in range(len(haystack) - n + 1):
        if haystack[i:i + n] == needle:
            return i
    return -1


def pack_clips(spec: dict) -> ClipSet:
    clips_spec = spec.get("clips")
    if not isinstance(clips_spec, list) or not clips_spec:
        raise Stage2dError("clips: a non-empty list of clips is required")
    if len(clips_spec) > 0xFFFF:
        raise Stage2dError("clips: at most 65535")
    sheet = spec.get("sheet")
    out = ClipSet()
    flat: list[tuple] = []  # the shape array as tuples, to find runs that are already stored
    names = set()
    for ci, cs in enumerate(clips_spec):
        name = cs.get("name")
        if not isinstance(name, str) or not name or name in names:
            raise Stage2dError(f"clip {ci}: needs a unique name")
        names.add(name)
        mode = MODES.get(cs.get("mode", "loop"))
        if mode is None:
            raise Stage2dError(f"{name}: mode must be one of {', '.join(MODES)}")
        frames_spec = cs.get("frames")
        if not isinstance(frames_spec, list) or not frames_spec:
            raise Stage2dError(f"{name}: needs at least one frame")
        if len(frames_spec) > 0xFFFF:
            raise Stage2dError(f"{name}: at most 65535 frames")
        loop_start = _int(cs.get("loop_start", 0), f"{name} loop_start", 0, len(frames_spec) - 1)
        frames = []
        for fi, fs in enumerate(frames_spec):
            what = f"{name} frame {fi}"
            source = _source(fs, sheet, what)
            shapes = []
            for si, ss in enumerate(fs.get("shapes", [])):
                sw = f"{what} shape {si}"
                shapes.append((_int(ss["x"], sw + " x", -32768, 32767), _int(ss["y"], sw + " y", -32768, 32767),
                               _int(ss.get("w", 0), sw + " w", 0, 65535), _int(ss.get("h", 0), sw + " h", 0, 65535),
                               _int(ss.get("kind", 0), sw + " kind", 0, 255), _int(ss.get("index", 0), sw + " index", 0, 255),
                               _int(ss.get("flags", 0), sw + " flags", 0, 65535)))
            if len(shapes) > 255:
                raise Stage2dError(f"{what}: at most 255 shapes per frame")
            first = _find_run(flat, shapes) if shapes else 0
            if first < 0:
                overlap = next((k for k in range(min(len(flat), len(shapes) - 1), 0, -1)
                                if flat[-k:] == shapes[:k]), 0)  # the stored tail may start this run
                first = len(flat) - overlap
                flat.extend(shapes[overlap:])
                out.shapes.extend(Shape(*s) for s in shapes[overlap:])
            if first > 0xFFFF:
                raise Stage2dError("more than 65535 shape entries")
            frames.append(Frame(source, _pivot(fs, source, what), _int(fs.get("duration", 1), what + " duration", 1, 65535),
                                _int(fs.get("event", 0), what + " event", 0, 65535), first, len(shapes),
                                _int(fs.get("flags", 0), what + " flags", 0, 255)))
        out.clips.append(Clip(name, mode, loop_start, frames))
    if len(out.shapes) > 0xFFFF:
        raise Stage2dError("more than 65535 shape entries")
    return out
