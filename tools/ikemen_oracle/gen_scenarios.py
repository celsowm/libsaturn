#!/usr/bin/env python3
"""Generate the KFM scenario matrix for the Ikemen oracle.

Every generated scenario walks P1 into contact range, performs one move and
declares an `expect` block. suite.py checks `expect` against the upstream
trace, so a scenario whose move never connects (a vacuous PASS) fails.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path

OUT = Path(__file__).resolve().parent / "scenarios"

BASE = {
    "p1": "../Ikemen-GO-Screenpack/chars/kfm/kfm.def",
    "p2": "../Ikemen-GO-Screenpack/chars/kfm/kfm.def",
    "stage": "../Ikemen-GO-Screenpack/stages/stage0.def",
    "seed": 1,
    "rounds": 1,
    "round_state": 2,
    "p1_ai": 0,
    "p2_ai": 0,
    "extra_args": [],
}

APPROACH = (10, 50)      # forward held: closes the 140 px gap
MOVE = 56                # first frame of the move input

# name -> (inputs after the approach, states P1 must visit, P2 life drops)
NORMALS = {
    "stand_x": ([(MOVE, MOVE, ["x"])], [200], True),
    "stand_y": ([(MOVE, MOVE, ["y"])], [210], True),
    "stand_a": ([(MOVE, MOVE, ["a"])], [230], True),
    "stand_b": ([(MOVE, MOVE, ["b"])], [240], True),
    "crouch_x": ([(MOVE - 6, MOVE + 6, ["down"]),
                  (MOVE, MOVE, ["down", "x"])], [400], True),
    "crouch_a": ([(MOVE - 6, MOVE + 6, ["down"]),
                  (MOVE, MOVE, ["down", "a"])], [430], True),
    "crouch_b": ([(MOVE - 6, MOVE + 6, ["down"]),
                  (MOVE, MOVE, ["down", "b"])], [440], True),
    "fireball_x": ([(MOVE, MOVE + 1, ["down"]),
                    (MOVE + 2, MOVE + 3, ["down", "forward"]),
                    (MOVE + 4, MOVE + 5, ["forward"]),
                    (MOVE + 6, MOVE + 6, ["forward", "x"])], [1000], False),
}

def motion(steps, button, start=MOVE):
    """Directional steps (two ticks each) then one button tick."""
    out, frame = [], start
    for keys in steps:
        out.append((frame, frame + 1, list(keys)))
        frame += 2
    out.append((frame, frame, list(keys_with(steps[-1], button))))
    return out

def keys_with(last, button):
    return [*last, *button] if button else list(last)

F, B, D = "forward", "back", "down"
QCF = [[D], [D, F], [F]]
QCB = [[D], [D, B], [B]]
UPPER = [[F], [D], [D, F]]

SPECIALS = {
    "qcf_y": (motion(QCF, ["y"]), None, True),
    "qcf_a": (motion(QCF, ["a"]), None, True),
    "qcf_b": (motion(QCF, ["b"]), None, True),
    "qcb_x": (motion(QCB, ["x"]), None, True),
    "qcb_y": (motion(QCB, ["y"]), None, True),
    "upper_x": (motion(UPPER, ["x"]), None, True),
    "upper_y": (motion(UPPER, ["y"]), None, True),
    "ff_a": ([(MOVE, MOVE, [F]), (MOVE + 2, MOVE + 2, [F]),
              (MOVE + 4, MOVE + 4, [F, "a"])], None, True),
    "run": ([(MOVE, MOVE, [F]), (MOVE + 2, MOVE + 40, [F])], None, False),
    "hop_back": ([(MOVE, MOVE, [B]), (MOVE + 2, MOVE + 2, [B])], None, False),
}

AIR = {  # neutral jump from contact range: button -> (state, frames later)
    "air_x": ("x", 600, 24), "air_y": ("y", 610, 27),
    "air_a": ("a", 630, 24), "air_b": ("b", 640, 24),
}

POWER = {"p1_power": 1000}

def two_step(first, second, button):
    """Two taps of `first`, then `second` + button (FF_a / FF_ab)."""
    return [(MOVE, MOVE, [first]), (MOVE + 2, MOVE + 2, [first]),
            (MOVE + 4, MOVE + 4, [second, *button])]

def double_qcf(button, start=MOVE):
    """~D, DF, F, D, DF, F, button (Triple Kung Fu Palm)."""
    return motion(QCF + QCF, button, start)

# Moves that cost power (setup gives P1 a full bar). name -> (moves, states,
# P2 life must drop)
SUPERS = {
    "palm_xy": (motion(QCF, ["x", "y"]), [1020], True),
    "upper_xy": (motion(UPPER, ["x", "y"]), [1120], True),
    "blow_xy": (motion(QCB, ["x", "y"]), [1220], True),
    "knee_ab": (two_step("forward", "forward", ["a", "b"]), [1070], True),
    "zankou_ab": (motion(QCF, ["a", "b"]), [1420], False),
    "triple_palm": (double_qcf(["x"]), [3000], True),
    "smash_upper": (motion([[D], [D, B], [B]] * 2, ["x"]), [3050], True),
}

# Cancels and chains: attack, then the next input while the first move is
# still active (movecontact / time windows from the CMD).
COMBOS = {
    # The attacker is frozen by hit pause until about MOVE + 12, so the
    # follow-up lands right after it.
    "chain_x_y": ([(MOVE, MOVE, ["x"]), (MOVE + 16, MOVE + 16, ["y"])],
                  [200, 210], True),
    "chain_x_a": ([(MOVE, MOVE, ["x"]), (MOVE + 18, MOVE + 18, ["a"])],
                  [200, 230], True),
    "rapid_x": ([(MOVE, MOVE, ["x"]), (MOVE + 16, MOVE + 16, ["x"]),
                 (MOVE + 34, MOVE + 34, ["x"])], [200], True),
    "cancel_x_qcf_y": ([(MOVE, MOVE, ["x"])] + motion(QCF, ["y"], MOVE + 6),
                       [200, 1010], True),
    "cancel_x_upper_x": ([(MOVE, MOVE, ["x"])] +
                         motion(UPPER, ["x"], MOVE + 6), [200, 1100], True),
    "cancel_y_blow_y": ([(MOVE, MOVE, ["y"])] + motion(QCB, ["y"], MOVE + 10),
                        [210, 1210], True),
}

# The same moves, performed by P2 against an idle P1.
P2_SIDE = {
    "stand_x": NORMALS["stand_x"],
    "crouch_a": NORMALS["crouch_a"],
    "qcf_y": SPECIALS["qcf_y"],
    "upper_y": SPECIALS["upper_y"],
}

# Both sides walk into range (frames 5..36); then P1 attacks at 42 while P2
# defends. name -> (P1 inputs after the approach, P2 inputs, P2 states).
def both_walk():
    return [{"from": 5, "to": 36, "p1": ["forward"], "p2": ["forward"]}]

def defence(p1_events, p2_events):
    out = both_walk()
    out += [{"from": a, "to": b, "p1": k} for a, b, k in p1_events]
    out += [{"from": a, "to": b, "p2": k} for a, b, k in p2_events]
    return out

DEFENCE = {
    # name: (P1 attack, P2 defence, P2 states to visit, frames)
    "guard_stand_y": ([(42, 42, ["y"])], [(38, 80, ["back"])], [130], 160),
    "guard_stand_b": ([(42, 42, ["b"])], [(38, 80, ["back"])], [130], 160),
    "guard_crouch_a": ([(38, 41, ["down"]), (42, 42, ["down", "a"])],
                       [(38, 80, ["down", "back"])], [131], 160),
    "guard_crouch_x": ([(38, 41, ["down"]), (42, 42, ["down", "x"])],
                       [(38, 80, ["down", "back"])], [131], 160),
}

def build_defence(name, p1_events, p2_events, states, frames):
    scenario = dict(BASE)
    scenario["name"] = name
    scenario["frames"] = frames
    scenario["inputs"] = defence(p1_events, p2_events)
    scenario["expect"] = {"p2_states": states}
    return name, scenario

def events(moves, approach=APPROACH, side="p1"):
    """P1 (or P2, with `side`) walks in, then performs `moves`."""
    other = "p2" if side == "p1" else "p1"
    out = [{"from": approach[0], "to": approach[1],
            side: ["forward"], other: []}]
    for start, end, keys in moves:
        out.append({"from": start, "to": end, side: keys, other: []})
    return out

def build(name, moves, states, p2_hit, frames, setup=None, side="p1"):
    """`states` are the states the acting side must visit; `p2_hit` says
    whether the opponent's life must drop (for side p2 that is P1)."""
    scenario = dict(BASE)
    scenario["name"] = name
    scenario["frames"] = frames
    scenario["inputs"] = events(moves, side=side)
    victim = "p2" if side == "p1" else "p1"
    scenario["expect"] = {f"{victim}_life_drops": p2_hit}
    if states:
        scenario["expect"][f"{side}_states"] = states
    if setup:
        scenario["setup"] = setup
    return name, scenario

ROUND_FLOW = ("needs the round flow after a KO: roundState 3, KO slow "
              "motion, win pose (docs/IKEMEN_COMPAT.md, stage F)")

def raw(name, inputs, frames, expect, setup=None, pending=None):
    scenario = dict(BASE)
    scenario["name"] = name
    scenario["frames"] = frames
    scenario["inputs"] = inputs
    scenario["expect"] = expect
    if setup:
        scenario["setup"] = setup
    if pending:
        scenario["pending"] = pending
    return name, scenario

def p1_events(*moves, p2=()):
    """P1 walks in, performs `moves`; `p2` are extra (from, to, keys) for P2."""
    out = events(list(moves))
    for a, b, k in p2:
        out.append({"from": a, "to": b, "p2": k})
    return out

def both_throw(keys, throws_side="p1"):
    """Both walk in until 42, then `throws_side` presses `keys` at 44."""
    out = [{"from": 5, "to": 42, "p1": [F], "p2": [F]}]
    out.append({"from": 44, "to": 44, throws_side: keys})
    return out

def extra():
    """Throw variants, blocking, recovery, KO, lying down and get-up."""
    yield raw("kfm_throw_back_160", both_throw([B, "y"]), 160,
              {"p1_states": [810], "p2_states": [820],
               "p2_life_drops": True})
    yield raw("kfm_p2_throw_160", both_throw([F, "y"], "p2"), 160,
              {"p2_states": [810], "p1_states": [820],
               "p1_life_drops": True})
    # `blocking` is F,x (3 ticks): the stand/crouch/air blocking states.
    yield raw("kfm_blocking_high_120",
              p1_events((MOVE, MOVE, [F]), (MOVE + 1, MOVE + 1, [F, "x"])),
              120, {"p1_states": [1300], "p2_life_drops": False})
    # Crouch first: a forward tap within 15 ticks of x would read as Upper.
    yield raw("kfm_blocking_low_120",
              p1_events((52, 69, [D]), (70, 70, [D, F]),
                        (71, 71, [D, F, "x"])),
              120, {"p1_states": [1320], "p2_life_drops": False})
    # P2 punches while P1 blocks it.
    inputs = [{"from": 5, "to": 34, "p1": [F], "p2": [F]},
              {"from": 35, "to": 36, "p1": [], "p2": [F]},
              {"from": 37, "to": 37, "p1": [F]},
              {"from": 38, "to": 38, "p1": [F, "x"], "p2": ["x"]}]
    yield raw("kfm_blocking_hit_160", inputs, 160,
              {"p1_states": [1300, 1310]})
    # Low and air versions of the blocking reversal (P1 blocks P2's attack).
    low = [{"from": 5, "to": 20, "p1": [F]}, {"from": 5, "to": 40, "p2": [F]},
           {"from": 35, "to": 41, "p1": [D]},
           {"from": 41, "to": 44, "p2": [D]},
           {"from": 45, "to": 45, "p2": [D, "a"]},
           {"from": 46, "to": 46, "p1": [D, F]},
           {"from": 47, "to": 47, "p1": [D, F, "x"]}]
    yield raw("kfm_blocking_low_hit_160", low, 160,
              {"p1_states": [1320, 1330]})
    air = [{"from": 5, "to": 34, "p1": [F], "p2": [F]},
           {"from": 33, "to": 35, "p1": ["up"]},
           {"from": 37, "to": 37, "p2": ["x"]},
           {"from": 37, "to": 37, "p1": [F]},
           {"from": 38, "to": 38, "p1": [F, "x"]}]
    # A standing punch is no air attack: the air block does not reverse it.
    yield raw("kfm_blocking_air_hit_160", air, 160,
              {"p1_states": [1340, 5020], "p1_life_drops": True})
    # Both jump; P2's air punch against P1's air block is reversed.
    airair = [{"from": 5, "to": 32, "p1": [F], "p2": [F]},
              {"from": 33, "to": 35, "p1": ["up"], "p2": ["up"]},
              {"from": 56, "to": 56, "p2": ["x"]},
              {"from": 58, "to": 58, "p1": [F]},
              {"from": 59, "to": 59, "p1": [F, "x"]}]
    yield raw("kfm_blocking_air_air_200", airair, 200,
              {"p1_states": [1350]})
    # P2 jumps and holds back in the air while P1 punches.
    airguard = [{"from": 5, "to": 32, "p1": [F], "p2": [F]},
                {"from": 33, "to": 35, "p2": ["up"]},
                {"from": 38, "to": 90, "p2": ["back"]},
                {"from": 38, "to": 38, "p1": ["x"]}]
    yield raw("kfm_guard_air_160", airguard, 160, {"p2_states": [132, 154]})
    # Upper launches P2; recovery (x+y) in the air, or lie down and get up.
    upper = [{"from": 10, "to": 50, "p1": [F], "p2": []}]
    upper += [{"from": a, "to": a + 1, "p1": k, "p2": []}
              for a, k in ((56, [F]), (58, [D]), (60, [D, F]))]
    upper.append({"from": 62, "to": 62, "p1": [D, F, "y"], "p2": []})
    yield raw("kfm_upper_getup_320", upper, 320,
              {"p2_states": [5100, 5110], "p2_life_drops": True})
    recover = [dict(e) for e in upper] + [
        # x+y must be a fresh press: tap it every third tick while falling.
        {"from": t, "to": t, "p2": ["x", "y"]} for t in range(104, 144, 3)]
    yield raw("kfm_recovery_200", recover, 200,
              {"p2_states": [5210], "p2_life_drops": True})
    # P2 starts with 1 life: a single punch knocks it out.
    yield raw("kfm_ko_stand_x_200",
              p1_events((MOVE, MOVE, ["x"])), 200,
              {"p2_states": [5150]}, setup={"p2_life": 1},
              pending=ROUND_FLOW)
    yield raw("kfm_ko_upper_y_240", upper, 240,
              {"p2_states": [5150]}, setup={"p2_life": 1},
              pending=ROUND_FLOW)

def matrix():
    for key, (moves, states, p2_hit) in P2_SIDE.items():
        yield build(f"kfm_p2_{key}_200", moves, states, p2_hit, 200,
                    side="p2")
    for key, (p1_ev, p2_ev, states, frames) in DEFENCE.items():
        yield build_defence(f"kfm_{key}_{frames}", p1_ev, p2_ev, states,
                            frames)
    yield from extra()
    for key, (moves, states, p2_hit) in SUPERS.items():
        yield build(f"kfm_super_{key}_200", moves, states, p2_hit, 200,
                    setup=POWER)
    for key, (moves, states, p2_hit) in COMBOS.items():
        yield build(f"kfm_combo_{key}_200", moves, states, p2_hit, 200)
    for key, (moves, states, p2_hit) in NORMALS.items():
        yield build(f"kfm_{key}_160", moves, states, p2_hit, 160)
    for key, (moves, states, p2_hit) in SPECIALS.items():
        yield build(f"kfm_{key}_200", moves, states, p2_hit, 200)
    for key, (button, state, delay) in AIR.items():
        moves = [(MOVE, MOVE + 2, ["up"]), (MOVE + delay, MOVE + delay, [button])]
        yield build(f"kfm_{key}_200", moves, [state], True, 200)

def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--out", type=Path, default=OUT)
    args = parser.parse_args()
    args.out.mkdir(parents=True, exist_ok=True)
    count = 0
    for name, scenario in matrix():
        path = args.out / f"{name}.json"
        path.write_text(json.dumps(scenario, indent=2) + "\n",
                        encoding="utf-8")
        count += 1
    print(f"wrote {count} scenarios to {args.out}")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
