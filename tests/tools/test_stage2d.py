#!/usr/bin/env python3
"""Host coverage for the stage2d offline tools (terrain profiles, metatiles, entity regions,
animation clips, Bezier tables, C emission, CLI). Run from the repository root."""

from __future__ import annotations

import json
import math
import random
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))

from stage2d import Stage2dError, build_stage, emit_c, load_spec  # noqa: E402
from stage2d import clips, entities, metatiles, paths, terrain  # noqa: E402
from stage2d.emit_c import size_report  # noqa: E402

FIXTURE = ROOT / "tests" / "fixtures" / "stage2d" / "synthetic_stage.json"
GENERATED = ROOT / "tests" / "host" / "fixtures"
TOOL = ROOT / "tools" / "stage2d_tool.py"


def committed(name: str) -> bytes:
    """A committed fixture's bytes with line endings normalised (a checkout may convert them)."""
    return (GENERATED / name).read_bytes().replace(b"\r\n", b"\n")


def raises(fn, *args, **kwargs) -> str:
    try:
        fn(*args, **kwargs)
    except Stage2dError as exc:
        return str(exc)
    raise AssertionError(f"{fn.__name__} did not raise")


# ----- terrain -----

def test_profiles():
    ramp = terrain.make_profile("ramp", [1, 2, 3, 4, 5, 6, 7, 8])
    # a full row is -8 (a run anchored to the left), exactly as sat_terrain_profile2_from_columns writes it
    assert ramp.row == (1, 2, 3, 4, 5, 6, 7, -8)
    block = terrain.make_profile("block", [8] * 8)
    assert block.row == (-8,) * 8
    ceiling = terrain.make_profile("ceiling", [-3] * 8)
    assert ceiling.row == (-8, -8, -8, 0, 0, 0, 0, 0)
    assert terrain.make_profile("empty", [0] * 8).row == (0,) * 8
    # a valley leaves a row with a hole: refused, with a message naming the row
    assert "hole" in raises(terrain.make_profile, "valley", [8, 0, 8, 8, 8, 8, 8, 8])
    assert "-8..8" in raises(terrain.make_profile, "tall", [9] * 8)
    assert "8 integers" in raises(terrain.make_profile, "short", [1, 2, 3])
    # flags, category, material range checks
    assert "category" in raises(terrain.make_profile, "bad", [1] * 8, 0, 0, 300)


def test_validate_catches_disagreement():
    p = terrain.make_profile("flat", [4] * 8)
    bad = terrain.Profile("bad", p.column, (8, 0, 0, 0, 0, 0, 0, 0))
    assert "disagree" in raises(terrain.validate_profile, bad)


def test_mask_round_trip():
    lines = ["........", "........", "........", "........", "....####", "...#####", "..######", ".#######"]
    mask = terrain.mask_from_text(lines)
    columns = terrain.columns_from_mask(mask)
    assert columns == (0, 1, 2, 3, 4, 4, 4, 4)  # bottom-anchored runs
    assert [[bool(c) for c in row] for row in terrain.mask_of(columns)] == mask
    floating = ["........"] * 3 + ["#......."] + ["........"] * 4
    assert "floats" in raises(terrain.columns_from_mask, terrain.mask_from_text(floating))
    gap = ["#......."] + ["........"] * 2 + ["#......."] + ["........"] * 4
    assert "gap" in raises(terrain.columns_from_mask, terrain.mask_from_text(gap))


def test_angles():
    assert terrain.derive_angle([4] * 8) == 0
    assert terrain.derive_angle([1, 2, 3, 4, 5, 6, 7, 8]) == 224  # 45 degrees up to the right
    assert terrain.derive_angle([8, 7, 6, 5, 4, 3, 2, 1]) == 32   # 45 degrees up to the left
    assert terrain.derive_angle([-4] * 8) == 128                   # a flat ceiling
    assert terrain.derive_angle([-1, -2, -3, -4, -5, -6, -7, -8]) == (128 - 224) & 255
    assert terrain.derive_angle([-1, -2, -3, -4, -5, -6, -7, 8]) == (128 - 224) & 255  # a full column is neutral
    assert "floor or ceiling" in raises(terrain.derive_angle, [4, 4, 4, 4, -4, -4, -4, -4])
    assert "floor or ceiling" in raises(terrain.derive_angle, [0, 4, 4, 4, 4, 4, 4, 4])


def test_transforms():
    ramp = terrain.make_profile("ramp", [1, 2, 3, 4, 5, 6, 7, 8], terrain.derive_angle([1, 2, 3, 4, 5, 6, 7, 8]), 0, 1, 9)
    for op in terrain.OPS:
        q = terrain.transform_profile(ramp, op)
        terrain.validate_profile(q)
        assert q.material == 9 and q.category == 1
    fx = terrain.transform_profile(ramp, "flip_x")
    assert fx.column == (8, 7, 6, 5, 4, 3, 2, 1) and fx.angle == terrain.derive_angle(list(fx.column))
    fy = terrain.transform_profile(ramp, "flip_y")
    assert fy.column == (-1, -2, -3, -4, -5, -6, -7, 8) and fy.angle == terrain.derive_angle(list(fy.column))
    # involutions and a full turn
    for op in ("flip_x", "flip_y"):
        twice = terrain.transform_profile(terrain.transform_profile(ramp, op), op)
        assert (twice.column, twice.row, twice.angle) == (ramp.column, ramp.row, ramp.angle)
    p = ramp
    for _ in range(4):
        p = terrain.transform_profile(p, "rot90")
    assert (p.column, p.row, p.angle) == (ramp.column, ramp.row, ramp.angle)
    assert terrain.transform_profile(ramp, "rot180").column == terrain.transform_profile(fy, "flip_x").column
    # a flat floor turned clockwise becomes a wall on the left whose travel direction is down
    wall = terrain.transform_profile(terrain.make_profile("f", [4] * 8), "rot90")
    assert wall.column == (8, 8, 8, 8, 0, 0, 0, 0) and wall.angle == 64
    assert "unknown transform" in raises(terrain.transform_profile, ramp, "mirror")


def test_compile_profiles():
    ps = terrain.compile_profiles([
        {"name": "block", "columns": [8] * 8, "category": 3},
        {"name": "slope", "columns": [1, 2, 3, 4, 5, 6, 7, 8], "angle": "auto", "variants": ["flip_x"], "flags": ["one_way", 16]},
        {"name": "down", "from": "slope", "transform": "flip_x", "material": 5},
        {"name": "wall", "mask": ["####....", "####....", "####....", "####....", "####....", "####....", "####....", "####...."]},
    ])
    names = [p.name for p in ps]
    assert names == ["empty", "block", "slope", "slope.flip_x", "down", "wall"]
    assert ps[2].flags == 0x11 and ps[2].angle == 224 and ps[3].angle == 32
    assert ps[4].column == ps[3].column and ps[4].material == 5 and ps[4].angle == 32
    assert ps[1].category == 3 and ps[5].category == 1  # default category 1
    assert ps[5].column == (8, 8, 8, 8, 0, 0, 0, 0)
    assert "used twice" in raises(terrain.compile_profiles, [{"name": "a", "columns": [1] * 8}, {"name": "a", "columns": [2] * 8}])
    assert "unknown flag" in raises(terrain.compile_profiles, [{"name": "a", "columns": [1] * 8, "flags": ["sticky"]}])
    assert "unknown base" in raises(terrain.compile_profiles, [{"name": "a", "from": "zzz"}])
    assert "needs columns" in raises(terrain.compile_profiles, [{"name": "a"}])
    many = [{"name": f"p{i}", "columns": [1] * 8} for i in range(1024)]
    assert "1024" in raises(terrain.compile_profiles, many)


# ----- metatiles -----

def test_metatiles():
    grid = [[1, 2, 1, 2], [3, 4, 3, 4], [1, 2, 1, 2], [3, 4, 3, 4]]
    ms = metatiles.compile_metatiles([grid], 1)
    assert ms.count == 2  # the all-fill metatile and the one repeating block
    assert ms.cells[:4] == [0, 0, 0, 0] and ms.cells[4:] == [1, 2, 3, 4]
    assert ms.layers[0].cols == 2 and ms.layers[0].rows == 2 and ms.layers[0].cells == [1, 1, 1, 1]
    # padding with the fill word, and no empty metatile first
    odd = [[7, 7, 7], [7, 7, 7], [7, 7, 7]]
    ms = metatiles.compile_metatiles([odd], 1, fill=9, empty_first=False)
    assert (ms.layers[0].cols, ms.layers[0].rows) == (2, 2)
    assert ms.cells[:4] == [7, 7, 7, 7] and ms.layers[0].cells[0] == 0 and ms.count == 4
    # layers share one table, numbered in first-seen order
    a = [[1, 1], [1, 1]]
    b = [[2, 2], [2, 2]]
    ms = metatiles.compile_metatiles([a, b, a], 1)
    assert [m.cells for m in ms.layers] == [[1], [2], [1]] and ms.count == 3
    # shift 0 is a one-cell metatile; limits
    ms = metatiles.compile_metatiles([[[5, 6]]], 0)
    assert ms.cells == [0, 5, 6] and ms.layers[0].cells == [1, 2]
    assert "outside 0..5" in raises(metatiles.compile_metatiles, [a], 6)
    assert "outside 0..4" in raises(metatiles.compile_metatiles, [a], 5, max_shift=4)
    assert "distinct" in raises(metatiles.compile_metatiles, [[[1, 2, 3, 4]]], 0, max_metatiles=3)
    assert "at least one" in raises(metatiles.compile_metatiles, [], 1)


# ----- entities -----

def brute_regions(items, shift, ox, oy, cols):
    keyed = sorted(range(len(items)), key=lambda i: (((items[i]["y"] - oy) >> shift) * cols + ((items[i]["x"] - ox) >> shift), i))
    return keyed


def test_entities():
    rng = random.Random(7)
    items = [{"x": rng.randrange(-300, 900), "y": rng.randrange(40, 700), "kind": rng.randrange(8), "data": rng.randrange(65536)}
             for _ in range(500)]
    ix = entities.build_index(items, 6)
    entities.validate_index(ix)
    assert ix.origin_x % 64 == 0 and ix.origin_y % 64 == 0 and ix.origin_x <= min(i["x"] for i in items)
    order = brute_regions(items, 6, ix.origin_x, ix.origin_y, ix.region_cols)
    got = [(d.x + ix.origin_x, d.y + ix.origin_y, d.kind, d.data) for d in ix.descs]
    assert got == [(items[i]["x"], items[i]["y"], items[i]["kind"], items[i]["data"]) for i in order]
    assert ix.region_start[0] == 0 and ix.region_start[-1] == 500
    # stable inside a region: two entities in one region keep the author's order
    two = entities.build_index([{"x": 5, "y": 5, "kind": 1}, {"x": 3, "y": 2, "kind": 2}], 5)
    assert [d.kind for d in two.descs] == [1, 2]
    # a pinned grid
    pinned = entities.build_index([{"x": 10, "y": 10}], 4, origin=(-16, -16), size=(4, 3))
    assert (pinned.origin_x, pinned.region_cols, pinned.region_rows, len(pinned.region_start)) == (-16, 4, 3, 13)
    assert pinned.descs[0].x == 26 and pinned.descs[0].y == 26
    # an empty index is still a valid one-region index
    empty = entities.build_index([], 6)
    entities.validate_index(empty)
    assert empty.region_start == [0, 0] and entities.index_bytes(0, 1) == 4
    # errors
    assert "outside the region grid" in raises(entities.build_index, [{"x": 99, "y": 0}], 4, origin=(0, 0), size=(2, 2))
    assert "65535" in raises(entities.build_index, [{"x": 0, "y": 0}, {"x": 70000, "y": 0}], 15, origin=(0, 0), size=(3, 1))
    assert "region_shift" in raises(entities.build_index, [], 2)
    assert "integer" in raises(entities.build_index, [{"x": 1.5, "y": 0}], 4)
    assert "0..65535" in raises(entities.build_index, [{"x": 1, "y": 0, "kind": 70000}], 4)
    assert "use a larger region_shift" in raises(entities.build_index, [{"x": 0, "y": 0}, {"x": 20000, "y": 20000}], 3)
    # a corrupt index is caught
    bad = entities.build_index([{"x": 1, "y": 1}, {"x": 40, "y": 1}], 5)
    bad.descs[0].x = 40
    assert "listed in region" in raises(entities.validate_index, bad)


# ----- clips -----

def clip_spec(frames, **extra):
    return {"clips": [{"name": "a", "frames": frames, **extra}], "sheet": {"cell_w": 8, "cell_h": 8, "columns": 4}}


def test_clips():
    spec = clip_spec([
        {"cell": 5, "duration": 2, "shapes": [{"x": 0, "y": 0, "w": 4, "h": 4}]},
        {"cell": 1, "pivot": "center", "event": 3, "shapes": [{"x": 0, "y": 0, "w": 4, "h": 4}, {"x": 9, "y": 9}]},
        {"source": [10, 20, 30, 40], "pivot": [1, 2], "shapes": [{"x": 9, "y": 9}]},
    ], mode="ping_pong")
    cs = clips.pack_clips(spec)
    f0, f1, f2 = cs.clips[0].frames
    assert f0.source == (8, 8, 8, 8) and f0.pivot == (4, 8) and f0.duration == 2  # cell 5 = column 1, row 1
    assert f1.source == (8, 0, 8, 8) and f1.pivot == (4, 4) and f1.event == 3
    assert f2.source == (10, 20, 30, 40) and f2.pivot == (1, 2) and cs.clips[0].mode == 2
    # the [A], [A, B] and [B] runs all live in the two-entry array [A, B]
    assert len(cs.shapes) == 2 and (f0.shape_first, f0.shape_count) == (0, 1)
    assert (f1.shape_first, f1.shape_count) == (0, 2) and (f2.shape_first, f2.shape_count) == (1, 1)
    assert cs.region_count == 3
    # a span of cells
    big = clips.pack_clips(clip_spec([{"cell": 0, "span": [2, 3]}]))
    assert big.clips[0].frames[0].source == (0, 0, 16, 24)
    # errors
    assert "duration" in raises(clips.pack_clips, clip_spec([{"cell": 0, "duration": 0}]))
    assert "needs source or cell" in raises(clips.pack_clips, clip_spec([{}]))
    assert "needs a top-level" in raises(clips.pack_clips, {"clips": [{"name": "a", "frames": [{"cell": 0}]}]})
    assert "loop_start" in raises(clips.pack_clips, clip_spec([{"cell": 0}], loop_start=1))
    assert "mode" in raises(clips.pack_clips, clip_spec([{"cell": 0}], mode="bounce"))
    assert "unique name" in raises(clips.pack_clips, {"clips": [{"name": "a", "frames": [{"source": [0, 0, 1, 1]}]}] * 2})
    assert "unknown pivot" in raises(clips.pack_clips, clip_spec([{"cell": 0, "pivot": "middle"}]))
    assert "source w" in raises(clips.pack_clips, clip_spec([{"source": [0, 0, 0, 4]}]))
    assert "at least one frame" in raises(clips.pack_clips, clip_spec([]))


# ----- paths -----

def test_paths():
    straight = paths.bezier_table([[0, 0], [10, 0], [20, 0], [30, 0]], 31)
    assert straight[0] == 0 and abs(straight[-1] - 30 * 65536) <= 2
    assert all(abs(straight[i] - round(i * 65536)) <= 2 for i in range(31))  # evenly spaced
    curve = paths.bezier_table([[0, 0], [40, 0], [40, 40], [80, 40]], 65)
    assert curve[0] == 0 and all(b >= a for a, b in zip(curve, curve[1:]))
    chord = math.hypot(80, 40)
    assert chord * 65536 < curve[-1] < (80 + 40) * 65536  # longer than the chord, shorter than the control polygon
    quad = paths.bezier_table([[0, 0], [30, 50], [60, 0]], 33)
    assert quad[-1] > 60 * 65536
    assert "3 (quadratic)" in raises(paths.bezier_table, [[0, 0], [1, 1]], 8)
    assert "entries" in raises(paths.bezier_table, [[0, 0], [1, 0], [2, 0]], 1)
    assert "non-zero" in raises(paths.bezier_table, [[5, 5], [5, 5], [5, 5]], 8)
    assert "32767" in raises(paths.bezier_table, [[0, 0], [40000, 0], [2, 0]], 8)


# ----- stage build, emission, CLI -----

def test_stage_build():
    stage = build_stage(load_spec(FIXTURE))
    assert [p.name for p in stage.profiles][:4] == ["empty", "block", "half", "half.flip_y"]
    t = stage.terrain
    assert (t.cols, t.rows, t.shift, t.outside) == (4, 2, 1, 1) and len(t.tiles.layers) == 2
    for w in t.tiles.cells:
        assert (w & 0x3FF) < len(stage.profiles)
    assert any(w & 0x0400 for w in t.tiles.cells)  # the flipped ramp
    assert stage.stage_map.tiles.count == 7 and len(stage.stage_map.tiles.layers) == 2
    assert len(stage.entities.descs) == 8 and len(stage.clips.clips) == 4 and len(stage.paths) == 2
    sizes = size_report(stage)
    assert sizes["profiles"] == 8 * 22 and sizes["entities"] == entities.index_bytes(8, 15)


def spec_with(**sections):
    base = {"name": "t", "profiles": [{"name": "b", "columns": [8] * 8}]}
    base.update(sections)
    return base


def test_stage_errors():
    layer = {"rows": ["bb", "bb"]}
    ok = spec_with(terrain={"legend": {"b": "b"}, "layers": [layer]})
    assert build_stage(ok).terrain.cols == 1
    assert "not in the legend" in raises(build_stage, spec_with(terrain={"legend": {}, "layers": [layer]}))
    assert "unknown profile" in raises(build_stage, spec_with(terrain={"legend": {"b": "nope"}, "layers": [layer]}))
    assert "unknown modifier" in raises(build_stage, spec_with(terrain={"legend": {"b": "b|zz"}, "layers": [layer]}))
    assert "same size" in raises(build_stage, spec_with(terrain={"legend": {"b": "b"}, "layers": [layer, {"rows": ["bbbbbbbb", "bbbbbbbb"]}]}))
    assert "1 to 4 layers" in raises(build_stage, spec_with(terrain={"legend": {"b": "b"}, "layers": [layer] * 5}))
    assert "row 1 has" in raises(build_stage, spec_with(terrain={"legend": {"b": "b"}, "layers": [{"rows": ["bb", "b"]}]}))
    assert "indexes profile" in raises(build_stage, spec_with(terrain={"layers": [{"rows": [[1, 700]]}]}))
    assert "lowercase" in raises(build_stage, {"name": "Bad Name"})
    assert "16-bit word" in raises(build_stage, spec_with(stage_map={"layers": [{"rows": [[70000]]}]}))
    assert "outside" in raises(build_stage, spec_with(terrain={"outside": "void", "legend": {"b": "b"}, "layers": [layer]}))
    assert "quadratic" in raises(build_stage, spec_with(paths=[{"name": "p", "kind": "line", "points": []}]))
    # a map word keeps its flip and user bits
    flip = build_stage(spec_with(terrain={"legend": {"b": "b|fx|fy|u5"}, "metatile_shift": 0, "layers": [{"rows": ["b"]}]}))
    assert flip.terrain.tiles.cells[1] == 1 | 0x0400 | 0x0800 | (5 << 12)


def test_emit_deterministic_and_matches_committed_fixture():
    spec = load_spec(FIXTURE)
    first = emit_c(build_stage(spec))
    second = emit_c(build_stage(json.loads(json.dumps(spec))))
    assert first == second
    header, source = first
    assert "\r" not in header and "\r" not in source and header.endswith("\n") and source.endswith("\n")
    assert committed("synth.h") == header.encode("utf-8"), "regenerate tests/host/fixtures/synth.h"
    assert committed("synth.c") == source.encode("utf-8"), "regenerate tests/host/fixtures/synth.c"
    # sections that are absent leave no symbols behind
    only = emit_c(build_stage({"name": "e", "entities": {"region_shift": 4, "items": []}}))
    assert "terrain2.h" not in only[0] and "e_entity_index" in only[0] and "e_entity_descs" not in only[1]


def run_cli(*args):
    return subprocess.run([sys.executable, str(TOOL), *map(str, args)], capture_output=True, text=True, cwd=ROOT)


def test_cli():
    with tempfile.TemporaryDirectory() as directory:
        out = Path(directory) / "out"
        r = run_cli("build", FIXTURE, "--out-dir", out)
        assert r.returncode == 0, r.stderr
        assert (out / "synth.h").read_bytes() == committed("synth.h")
        assert (out / "synth.c").read_bytes() == committed("synth.c")
        assert "total" in r.stdout
        r = run_cli("check", FIXTURE)
        assert r.returncode == 0 and "synth" in r.stdout
        r = run_cli("check", FIXTURE, "--max-bytes", "100")
        assert r.returncode == 1 and "exceeds the budget" in r.stderr
        bad = Path(directory) / "bad.json"
        bad.write_text(json.dumps(spec_with(terrain={"legend": {}, "layers": [{"rows": ["x"]}]})), encoding="utf-8")
        r = run_cli("check", bad)
        assert r.returncode == 1 and "not in the legend" in r.stderr and "Traceback" not in r.stderr
        notjson = Path(directory) / "x.json"
        notjson.write_text("{", encoding="utf-8")
        r = run_cli("check", notjson)
        assert r.returncode == 1 and "not valid JSON" in r.stderr


def test_provenance_boundary():
    """The generic tools and their committed fixtures name no reference game and read no external tree."""
    files = [p for p in (ROOT / "tools" / "stage2d").glob("*.py")] + [TOOL, FIXTURE, GENERATED / "synth.c", GENERATED / "synth.h"]
    for path in files:
        text = path.read_text(encoding="utf-8").lower()
        for banned in (".external", "sa2", "sonic", "sega_saturn_hardware"):
            assert banned not in text, f"{path.name} mentions {banned}"


def main() -> int:
    tests = [(n, f) for n, f in sorted(globals().items()) if n.startswith("test_") and callable(f)]
    for name, fn in tests:
        fn()
        print(f"ok {name}")
    print(f"test_stage2d: {len(tests)} passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
