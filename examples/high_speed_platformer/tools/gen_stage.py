#!/usr/bin/env python3
"""Writes the stage2d spec and the game layout of the high_speed_platformer example.

The stage is synthetic: every tile, ring and platform is placed by the code below, so nothing in it
comes from another game. Run through the Makefile as

    python examples/high_speed_platformer/tools/gen_stage.py --out-dir build/generated/high_speed_platformer

which writes `stage_spec.json` (compiled by tools/stage2d_tool.py into stage.h / stage.c) and
`layout.h` / `layout.c`: the positions of everything the game places by hand (spawn point, layer
triggers, moving platforms, the rail, the finish), so that the world and the code never disagree.

Terrain is built in tile units (8 px). The ground is a strip of columns that rise and fall with a
few slope kinds; the loop is an octagon whose right half lives on terrain layer 0 and whose left
half lives on layer 1, so a runner that enters on layer 0, switches layer at the top and leaves on
layer 1 passes through the entry side of the ring.
"""

from __future__ import annotations

import argparse
import json
import math
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(REPO / "tools"))

from stage2d.terrain import compile_profiles  # noqa: E402

COLS, ROWS = 384, 48          # the stage in tiles: 3072 x 384 px
G = 40                         # row of the flat ground's top surface
TILE = 8

# entity kinds (the game's own numbering)
KIND_RING, KIND_DASH, KIND_SPRING, KIND_CHECKPOINT, KIND_GOAL = 1, 2, 3, 4, 5

PROFILES = [
    {"name": "full", "columns": [8] * 8, "angle": 0, "category": 1, "material": 1,
     "variants": []},
    {"name": "top", "columns": [8] * 8, "angle": 0, "category": 1, "material": 2},
    {"name": "ceil", "from": "full", "transform": "flip_y"},
    {"name": "wall_r", "from": "full", "transform": "rot270"},
    {"name": "wall_l", "from": "full", "transform": "rot90"},
    {"name": "r45", "columns": [1, 2, 3, 4, 5, 6, 7, 8], "angle": "auto", "category": 1, "material": 2},
    {"name": "d45", "from": "r45", "transform": "flip_x"},
    {"name": "cs_r", "from": "r45", "transform": "flip_y"},
    {"name": "cs_l", "from": "r45", "transform": ["flip_x", "flip_y"]},
    {"name": "g22a", "columns": [1, 1, 2, 2, 3, 3, 4, 4], "angle": "auto", "category": 1, "material": 2},
    {"name": "g22b", "columns": [5, 5, 6, 6, 7, 7, 8, 8], "angle": "auto", "category": 1, "material": 2},
    {"name": "d22a", "from": "g22a", "transform": "flip_x"},
    {"name": "d22b", "from": "g22b", "transform": "flip_x"},
    {"name": "plank", "columns": [-2] * 8, "angle": 0, "flags": ["one_way"], "category": 1, "material": 3},
]

# one character per profile, shared by the terrain and the visual layers
LEGEND = {
    ".": "empty", "#": "full", "T": "top", "c": "ceil", "W": "wall_r", "w": "wall_l",
    "/": "r45", "\\": "d45", ">": "cs_r", "<": "cs_l",
    "A": "g22a", "B": "g22b", "D": "d22b", "E": "d22a", "p": "plank",
}
DECOR = {"1": "cloud_l", "2": "cloud_m", "3": "cloud_r"}  # characters after the profiles


class Canvas:
    def __init__(self, cols, rows, fill="."):
        self.cols, self.rows = cols, rows
        self.cells = [[fill] * cols for _ in range(rows)]

    def put(self, col, row, ch):
        if 0 <= col < self.cols and 0 <= row < self.rows:
            self.cells[row][col] = ch

    def get(self, col, row):
        return self.cells[row][col]

    def fill(self, col0, row0, col1, row1, ch):
        for r in range(row0, row1):
            for c in range(col0, col1):
                self.put(c, r, ch)

    def rows_text(self):
        return ["".join(r) for r in self.cells]


class Ground:
    """A strip of columns whose surface rises and falls. Tracks the surface height of every column
    so that rings and entities can sit on it."""

    def __init__(self):
        self.canvases = [Canvas(COLS, ROWS), Canvas(COLS, ROWS)]
        self.rise = 0            # rows above G of the current flat surface
        self.col = 0
        self.surface = [G * TILE] * COLS   # y of the surface at the middle of each column

    def _put_both(self, col, row, ch):
        for cv in self.canvases:
            cv.put(col, row, ch)

    def _fill_below(self, col, top_row):
        for r in range(top_row, ROWS):
            self._put_both(col, r, "#")

    def flat(self, n):
        for _ in range(n):
            row = G - self.rise
            self._put_both(self.col, row, "T")
            self._fill_below(self.col, row + 1)
            self.surface[self.col] = row * TILE
            self.col += 1

    def up45(self, n):
        for _ in range(n):
            row = G - self.rise - 1
            self._put_both(self.col, row, "/")
            self._fill_below(self.col, row + 1)
            self.surface[self.col] = (row + 1) * TILE - 4
            self.rise += 1
            self.col += 1

    def down45(self, n):
        for _ in range(n):
            row = G - self.rise
            self._put_both(self.col, row, "\\")
            self._fill_below(self.col, row + 1)
            self.surface[self.col] = row * TILE + 4
            self.rise -= 1
            self.col += 1

    def up22(self, pairs):
        for _ in range(pairs):
            row = G - self.rise - 1
            for ch, y in (("A", (row + 1) * TILE - 2), ("B", row * TILE + 4)):
                self._put_both(self.col, row, ch)
                self._fill_below(self.col, row + 1)
                self.surface[self.col] = y
                self.col += 1
            self.rise += 1

    def down22(self, pairs):
        for _ in range(pairs):
            row = G - self.rise
            for ch, y in (("D", row * TILE + 2), ("E", (row + 1) * TILE - 4)):
                self._put_both(self.col, row, ch)
                self._fill_below(self.col, row + 1)
                self.surface[self.col] = y
                self.col += 1
            self.rise -= 1

    def pit(self, n):
        for _ in range(n):
            self.surface[self.col] = ROWS * TILE + 64
            self.col += 1


def build_ground():
    g = Ground()
    g.flat(56)
    g.up22(3)
    g.flat(4)
    g.up45(4)
    g.flat(6)
    g.down45(7)
    g.flat(11)
    pit1 = (g.col, g.col + 40)
    g.pit(40)
    g.flat(60)
    g.flat(36)                  # plank section sits on flat ground
    g.up22(2)
    g.flat(4)
    pit2 = (g.col, g.col + 36)
    g.pit(36)
    g.flat(46)
    g.up22(2)
    g.flat(4)
    g.down22(2)
    g.flat(COLS - g.col)
    assert g.col == COLS, g.col
    for cv in g.canvases:                # a wall at each end of the stage
        cv.fill(0, 0, 2, ROWS, "#")
        cv.fill(COLS - 2, 0, COLS, ROWS, "#")
    return g, pit1, pit2


LOOP_X0 = 156


def build_loop(g):
    """An octagon, 16 x 16 tiles inside. Right half on terrain layer 0, left half on layer 1."""
    x0 = LOOP_X0
    c0, c1 = g.canvases
    # the mass above the ring is on both layers, with the flat ceiling as its surface
    for cv in (c0, c1):
        cv.fill(x0 - 4, G - 24, x0 + 20, G - 17, "#")
        cv.fill(x0 + 4, G - 17, x0 + 12, G - 16, "c")
        cv.fill(x0 + 4, G - 17, x0 + 12, G - 16, "c")
    # right half: terrain layer 0
    for i in range(4):
        col = x0 + 12 + i
        c0.put(col, G - 1 - i, "/")
        c0.fill(col, G - i, col + 1, G, "#")
    c0.fill(x0 + 16, G - 20, x0 + 20, G, "#")
    c0.fill(x0 + 16, G - 12, x0 + 17, G - 4, "W")
    for i in range(4):
        col, row = x0 + 15 - i, G - 13 - i
        c0.put(col, row, ">")
        c0.fill(col + 1, row, x0 + 16, row + 1, "#")
        c0.fill(col, G - 17, col + 1, row, "#")
    # left half: terrain layer 1
    for i in range(4):
        col = x0 + 3 - i
        c1.put(col, G - 1 - i, "\\")
        c1.fill(col, G - i, col + 1, G, "#")
    c1.fill(x0 - 4, G - 20, x0, G, "#")
    c1.fill(x0 - 1, G - 12, x0, G - 4, "w")
    for i in range(4):
        col, row = x0 + i, G - 13 - i
        c1.put(col, row, "<")
        c1.fill(x0, row, col, row + 1, "#")
        c1.fill(col, G - 17, col + 1, row, "#")
    # the strip of ground inside the ring stays flat on both layers (already built)


def octagon_points():
    """The centreline of the ring, 14 px inside the surface, in world pixels."""
    x0 = LOOP_X0 * TILE
    top = (G - 16) * TILE
    bottom = G * TILE
    inset = 14
    left, right = x0 + inset, x0 + 16 * TILE - inset
    chamfer = 4 * TILE
    return [
        (x0 + chamfer, bottom - inset), (x0 + 16 * TILE - chamfer, bottom - inset),
        (right, bottom - chamfer), (right, top + chamfer),
        (x0 + 16 * TILE - chamfer, top + inset), (x0 + chamfer, top + inset),
        (left, top + chamfer), (left, bottom - chamfer),
    ]


def ring_line(items, x0, x1, step, y_of):
    x = x0
    while x <= x1:
        items.append({"x": int(x), "y": int(y_of(x)), "kind": KIND_RING})
        x += step


def build_entities(g, pit1, pit2):
    items = []
    gy = lambda x: g.surface[min(COLS - 1, max(0, int(x) // TILE))] - 18
    # section A: a line and a dash pad
    ring_line(items, 8 * TILE, 40 * TILE, 16, gy)
    items.append({"x": 46 * TILE, "y": G * TILE - 3, "kind": KIND_DASH, "data": 1})
    ring_line(items, 60 * TILE, 70 * TILE, 16, gy)
    # the hill: rings along the plateau and the down slope
    ring_line(items, 66 * TILE, 92 * TILE, 16, gy)
    items.append({"x": 88 * TILE, "y": G * TILE - 3, "kind": KIND_CHECKPOINT, "data": 1})
    # above the first pit
    p0 = pit1[0] * TILE
    for k in range(10):
        items.append({"x": p0 + 24 + k * 28, "y": G * TILE - 70 - int(30 * math.sin(k * 0.7)), "kind": KIND_RING})
    # the dash pad before the loop and rings around the ring
    items.append({"x": (LOOP_X0 - 14) * TILE, "y": G * TILE - 3, "kind": KIND_DASH, "data": 1})
    items.append({"x": (LOOP_X0 - 18) * TILE, "y": G * TILE - 3, "kind": KIND_CHECKPOINT, "data": 2})
    pts = octagon_points()
    for k in range(8):
        a, b = pts[k], pts[(k + 1) % 8]
        n = max(2, int(math.hypot(b[0] - a[0], b[1] - a[1]) // 18))
        for j in range(n):
            t = j / n
            items.append({"x": int(a[0] + (b[0] - a[0]) * t), "y": int(a[1] + (b[1] - a[1]) * t), "kind": KIND_RING})
    ring_line(items, (LOOP_X0 + 24) * TILE, (LOOP_X0 + 40) * TILE, 16, gy)
    # planks and a spring
    plank_cols = [(200, 208, G - 5), (212, 220, G - 10), (224, 232, G - 6)]
    for a, b, row in plank_cols:
        ring_line(items, a * TILE + 8, b * TILE - 8, 16, lambda x, r=row: r * TILE - 12)
    items.append({"x": 210 * TILE, "y": G * TILE - 3, "kind": KIND_SPRING, "data": 11})
    # rail section
    ring_line(items, 231 * TILE, 236 * TILE, 16, gy)
    ring_line(items, 278 * TILE, 330 * TILE, 16, gy)
    items.append({"x": 300 * TILE, "y": G * TILE - 3, "kind": KIND_DASH, "data": 1})
    items.append({"x": 340 * TILE, "y": G * TILE - 3, "kind": KIND_CHECKPOINT, "data": 3})
    ring_line(items, 346 * TILE, 368 * TILE, 16, gy)
    items.append({"x": GOAL_COL * TILE, "y": G * TILE, "kind": KIND_GOAL})
    return items


GOAL_COL = 372

# --- clips ---------------------------------------------------------------------------------------
SHAPE_BODY, SHAPE_PICKUP = 1, 2


def clips_spec():
    body = [{"x": -6, "y": -24, "w": 12, "h": 24, "kind": SHAPE_BODY}]
    pickup = [{"x": -8, "y": -8, "w": 16, "h": 16, "kind": SHAPE_PICKUP}]
    clips = [
        {"name": "idle", "mode": "loop", "frames": [
            {"cell": 0, "duration": 30, "pivot": "bottom_center", "shapes": body}]},
        {"name": "run", "mode": "loop", "frames": [
            {"cell": 1 + i, "duration": 3, "pivot": "bottom_center", "event": 1 if i % 2 == 0 else 0, "shapes": body}
            for i in range(4)]},
        {"name": "roll", "mode": "loop", "frames": [
            {"cell": 5 + i, "duration": 2, "pivot": "bottom_center", "shapes": body} for i in range(4)]},
        {"name": "ring", "mode": "loop", "frames": [
            {"source": [16 * i, 64, 16, 16], "duration": 6, "pivot": "center", "shapes": pickup} for i in range(4)]},
        {"name": "sparkle", "mode": "once", "frames": [
            {"source": [64 + 16 * i, 64, 16, 16], "duration": 3, "pivot": "center",
             "event": 2 if i == 2 else 0} for i in range(3)]},
        {"name": "spring", "mode": "once", "frames": [
            {"source": [112, 64, 16, 16], "duration": 60, "pivot": "bottom_center"}]},
        {"name": "sprung", "mode": "once", "frames": [
            {"source": [128, 64, 16, 16], "duration": 12, "pivot": "bottom_center"},
            {"source": [112, 64, 16, 16], "duration": 1, "pivot": "bottom_center"}]},
        {"name": "flag", "mode": "loop", "frames": [
            {"source": [144, 64, 16, 32], "duration": 10, "pivot": "bottom_center"},
            {"source": [160, 64, 16, 32], "duration": 10, "pivot": "bottom_center"}]},
        {"name": "dash", "mode": "loop", "frames": [
            {"source": [176, 64, 16, 8], "duration": 4, "pivot": "bottom_center"},
            {"source": [192, 64, 16, 8], "duration": 4, "pivot": "bottom_center"}]},
    ]
    return {"sheet": {"cell_w": 32, "cell_h": 32, "columns": 8}, "clips": clips}


def build_visual(g, index):
    """Foreground = both terrain layers merged; two parallax layers behind it."""
    fg = Canvas(COLS, ROWS)
    for r in range(ROWS):
        for c in range(COLS):
            a, b = g.canvases[0].get(c, r), g.canvases[1].get(c, r)
            fg.put(c, r, a if a != "." else b)
    # hills: a gentle rolling strip made with the same slope tiles, tiled with wrap
    ops = [("flat", 6), ("up22", 2), ("flat", 3), ("down22", 2), ("flat", 5), ("up45", 2), ("flat", 3),
           ("down45", 2), ("flat", 64 - 6 - 4 - 3 - 4 - 5 - 2 - 3 - 2)]
    hills = Canvas(64, ROWS)
    col = 0
    rise = 0
    base = 36
    for op, n in ops:
        for _ in range(n):
            if op == "flat":
                hills.put(col, base - rise, "T")
                row = base - rise
                col_fill(hills, col, row + 1, "#")
                col += 1
            elif op == "up45":
                row = base - rise - 1
                hills.put(col, row, "/")
                col_fill(hills, col, row + 1, "#")
                rise += 1
                col += 1
            elif op == "down45":
                row = base - rise
                hills.put(col, row, "\\")
                col_fill(hills, col, row + 1, "#")
                rise -= 1
                col += 1
            elif op == "up22":
                row = base - rise - 1
                for ch in ("A", "B"):
                    hills.put(col, row, ch)
                    col_fill(hills, col, row + 1, "#")
                    col += 1
                rise += 1
            elif op == "down22":
                row = base - rise
                for ch in ("D", "E"):
                    hills.put(col, row, ch)
                    col_fill(hills, col, row + 1, "#")
                    col += 1
                rise -= 1
    assert col == 64 and rise == 0, (col, rise)
    clouds = Canvas(64, ROWS)
    for cx, cy, w in ((4, 8, 5), (20, 12, 4), (33, 6, 6), (47, 14, 4), (58, 9, 5)):
        clouds.put(cx, cy, "1")
        for k in range(1, w - 1):
            clouds.put(cx + k, cy, "2")
        clouds.put(cx + w - 1, cy, "3")
    return fg, hills, clouds


def col_fill(cv, col, row, ch):
    for r in range(row, cv.rows):
        cv.put(col, r, ch)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--out-dir", required=True)
    args = parser.parse_args()
    out = Path(args.out_dir)
    out.mkdir(parents=True, exist_ok=True)

    g, pit1, pit2 = build_ground()
    build_loop(g)

    profiles = compile_profiles(PROFILES)
    index = {p.name: i for i, p in enumerate(profiles)}
    legend_terrain = {ch: ("." if name == "empty" else name) for ch, name in LEGEND.items()}
    # planks: one-way tiles at the plank rows on both layers
    for a, b, row in [(200, 208, G - 5), (212, 220, G - 10), (224, 232, G - 6)]:
        for cv in g.canvases:
            cv.fill(a, row, b, row + 1, "p")

    fg, hills, clouds = build_visual(g, index)
    first_decor = len(profiles)
    legend_map = {ch: index[name] for ch, name in LEGEND.items() if name != "empty"}
    legend_map["."] = 0
    for k, (ch, name) in enumerate(DECOR.items()):
        legend_map[ch] = first_decor + k

    spec = {
        "name": "stage",
        "profiles": PROFILES,
        "terrain": {
            "metatile_shift": 2,
            "outside": "empty",
            "legend": legend_terrain,
            "layers": [{"rows": g.canvases[0].rows_text()}, {"rows": g.canvases[1].rows_text()}],
        },
        "stage_map": {
            "shift": 2,
            "fill": 0,
            "legend": legend_map,
            "layers": [{"rows": fg.rows_text()}, {"rows": hills.rows_text()}, {"rows": clouds.rows_text()}],
        },
        "entities": {
            "region_shift": 7,
            "origin": [0, 0],
            "size": [COLS * TILE >> 7, ROWS * TILE >> 7],
            "items": build_entities(g, pit1, pit2),
        },
        "clips": clips_spec(),
        "paths": [
            {"name": "rail", "kind": "cubic",
             "points": [[RAIL[0][0], RAIL[0][1]], [RAIL[1][0], RAIL[1][1]], [RAIL[2][0], RAIL[2][1]], [RAIL[3][0], RAIL[3][1]]],
             "table_entries": 129},
            {"name": "swing", "kind": "quadratic",
             "points": [list(p) for p in SWING], "table_entries": 65},
        ],
    }
    (out / "stage_spec.json").write_text(json.dumps(spec, indent=1) + "\n", encoding="utf-8", newline="\n")
    write_layout(out, g, pit1, pit2, len(profiles), first_decor)


# rail: a cubic over the second pit; a swinging platform: a quadratic over the first one
RAIL = [(236 * TILE, (G - 7) * TILE), (248 * TILE, (G - 25) * TILE), (266 * TILE, (G - 25) * TILE),
        (276 * TILE, (G - 7) * TILE)]
SWING = [(780, (G - 2) * TILE - 4), (850, (G - 15) * TILE), (920, (G - 2) * TILE - 4)]


def write_layout(out, g, pit1, pit2, profile_count, first_decor):
    x0 = LOOP_X0 * TILE
    top = (G - 16) * TILE
    triggers = [
        # x, y, w, h, layer, dir (1 right, -1 left, 0 either)
        (x0 - 64, (G - 8) * TILE, 16, 8 * TILE, 0, 0),      # before the ring: layer 0
        (x0 + 7 * TILE, top, 3 * TILE, 5 * TILE, 1, -1),     # on the ceiling, heading left: layer 1
        (x0 + 24 * TILE, (G - 8) * TILE, 16, 8 * TILE, 0, 0),  # after the ring: back to layer 0
    ]
    platforms = [
        # kind 0: follows the `swing` Bezier, back and forth; kind 1: rides a circle
        (0, 20, 4, 1, 0, 0, 0, 24),
        (1, 20, 4, 0, 1000, 270, 40, 20),
    ]
    h = ["/* Generated by examples/high_speed_platformer/tools/gen_stage.py. Do not edit. */",
         "#ifndef HSP_LAYOUT_H", "#define HSP_LAYOUT_H", "", "#include <stdint.h>", "",
         "#ifdef __cplusplus", 'extern "C" {', "#endif", "",
         f"#define HSP_STAGE_W {COLS * TILE}", f"#define HSP_STAGE_H {ROWS * TILE}",
         f"#define HSP_GROUND_Y {G * TILE}", f"#define HSP_DEATH_Y {ROWS * TILE + 48}",
         "#define HSP_START_X 40", f"#define HSP_START_Y {G * TILE}",
         f"#define HSP_GOAL_X {GOAL_COL * TILE}",
         f"#define HSP_ROOM_X {344 * TILE}   /* the camera is locked to the finish room from here */",
         f"#define HSP_PIT1_X0 {pit1[0] * TILE}", f"#define HSP_PIT1_X1 {pit1[1] * TILE}",
         f"#define HSP_PIT2_X0 {pit2[0] * TILE}", f"#define HSP_PIT2_X1 {pit2[1] * TILE}",
         f"#define HSP_LOOP_X {LOOP_X0 * TILE}", f"#define HSP_PROFILE_COUNT {profile_count}u",
         f"#define HSP_DECOR_FIRST {first_decor}u", "",
         "#define HSP_KIND_RING 1", "#define HSP_KIND_DASH 2", "#define HSP_KIND_SPRING 3",
         "#define HSP_KIND_CHECKPOINT 4", "#define HSP_KIND_GOAL 5", "",
         "#define HSP_SHAPE_BODY 1", "#define HSP_SHAPE_PICKUP 2", "",
         "typedef struct hsp_trigger {", "    int16_t x, y, w, h;", "    uint8_t layer;", "    int8_t dir;",
         "    uint8_t reserved[2];", "} hsp_trigger_t;", "",
         f"#define HSP_TRIGGER_COUNT {len(triggers)}u", "extern const hsp_trigger_t hsp_triggers[HSP_TRIGGER_COUNT];", "",
         "/* path_kind 0 follows the Bezier `swing` back and forth, 1 rides a circle (cx, cy, radius) */",
         "typedef struct hsp_platform {", "    uint8_t path_kind;", "    uint8_t half_w, half_h;", "    uint8_t one_way;",
         "    int16_t cx, cy, radius;", "    int16_t speed_q4;  /* path speed in 1/16 px per step */", "} hsp_platform_t;", "",
         f"#define HSP_PLATFORM_COUNT {len(platforms)}u",
         "extern const hsp_platform_t hsp_platforms[HSP_PLATFORM_COUNT];", "",
         "#ifdef __cplusplus", "}", "#endif", "", "#endif /* HSP_LAYOUT_H */", ""]
    c = ["/* Generated by examples/high_speed_platformer/tools/gen_stage.py. Do not edit. */",
         '#include "layout.h"', "",
         "const hsp_trigger_t hsp_triggers[HSP_TRIGGER_COUNT] = {"]
    for t in triggers:
        c.append("    {%d, %d, %d, %d, %d, %d, {0, 0}}," % t)
    c += ["};", "", "const hsp_platform_t hsp_platforms[HSP_PLATFORM_COUNT] = {"]
    for p in platforms:
        kind, hw, hh, one_way, cx, cy, radius, speed = p
        c.append("    {%d, %d, %d, %d, %d, %d, %d, %d}," % (kind, hw, hh, one_way, cx, cy, radius, speed))
    c += ["};", ""]
    (out / "layout.h").write_text("\n".join(h), encoding="utf-8", newline="\n")
    (out / "layout.c").write_text("\n".join(c), encoding="utf-8", newline="\n")


if __name__ == "__main__":
    main()
