#!/usr/bin/env python3
from __future__ import annotations

import json
import tempfile
from pathlib import Path

from tools.ikemen_oracle.diff import (
    DEFAULT_IGNORED_FIELDS,
    compare,
    load_jsonl,
)
from tools.ikemen_oracle.install import (
    BEGIN_MARKER,
    END_MARKER,
    install,
    uninstall,
)
from tools.ikemen_oracle.inputs import build_timeline
from tools.ikemen_oracle.run import (
    apply_upstream_build_env,
    build_command,
    load_scenario,
    missing_prerequisites,
    stage_screenpack,
)

ROOT = Path(__file__).resolve().parents[2]

def test_install_roundtrip() -> None:
    source = """package main

func (s *System) runMatch() bool {
	// Setup characters
	s.SetupCharRoundStart()

	for {
		// Update game state
		s.action()
	}
}
"""
    with tempfile.TemporaryDirectory() as td:
        root = Path(td) / "Ikemen-GO"
        src = root / "src"
        src.mkdir(parents=True)
        system_go = src / "system.go"
        system_go.write_text(source, encoding="utf-8")
        input_go = src / "input.go"
        input_source = """package main

func (cl *CommandList) InputUpdate(char *Char, controller int) bool {
	var buttons [14]bool
	var axes [6]float32
	_ = buttons
	_ = axes
	// Convert bool slice back to named inputs
	return true
}
"""
        input_go.write_text(input_source, encoding="utf-8")

        hook = Path(td) / "oracle_hook.go"
        hook.write_text("package main\n", encoding="utf-8")

        install(root, hook)
        patched = system_go.read_text(encoding="utf-8")
        assert patched.count(BEGIN_MARKER) == 2
        assert patched.count(END_MARKER) == 2
        patched_input = input_go.read_text(encoding="utf-8")
        assert patched_input.count(BEGIN_MARKER) == 1
        assert patched_input.count(END_MARKER) == 1
        assert (src / "libsaturn_oracle.go").is_file()

        install(root, hook)
        patched2 = system_go.read_text(encoding="utf-8")
        assert patched2.count(BEGIN_MARKER) == 2
        assert patched2.count(END_MARKER) == 2
        patched_input2 = input_go.read_text(encoding="utf-8")
        assert patched_input2.count(BEGIN_MARKER) == 1
        assert patched_input2.count(END_MARKER) == 1

        uninstall(root)
        assert system_go.read_text(encoding="utf-8") == source
        assert input_go.read_text(encoding="utf-8") == input_source
        assert not (src / "libsaturn_oracle.go").exists()


def test_input_timeline() -> None:
    scenario = {
        "frames": 6,
        "inputs": [
            {"from": 1, "to": 2, "p1": ["forward"], "p2": ["back"]},
            {"frame": 4, "p1": ["x", "down"], "p2": []},
        ],
    }
    timeline = build_timeline(scenario)
    assert timeline == [
        (0, 0),
        (1 << 0, 1 << 1),
        (1 << 0, 1 << 1),
        (0, 0),
        ((1 << 7) | (1 << 3), 0),
        (0, 0),
    ]

def test_scenario_and_command() -> None:
    with tempfile.TemporaryDirectory() as td:
        scenario_path = Path(td) / "scenario.json"
        scenario_path.write_text(
            json.dumps({
                "p1": "kfm",
                "p2": "kfm",
                "stage": "stages/training.def",
                "frames": 12,
                "seed": 7,
                "p1_ai": 0,
                "p2_ai": 0,
            }),
            encoding="utf-8",
        )
        scenario = load_scenario(scenario_path)
        cmd = build_command(scenario)
        assert cmd[:3] == ["go", "run", "./src"]
        assert cmd[cmd.index("-p1") + 1] == "kfm"
        assert cmd[cmd.index("-p2") + 1] == "kfm"
        assert cmd[cmd.index("-s") + 1] == "stages/training.def"
        assert cmd[cmd.index("-p1.ai") + 1] == "0"
        assert cmd[cmd.index("-p2.ai") + 1] == "0"

def test_trace_diff() -> None:
    with tempfile.TemporaryDirectory() as td:
        a = Path(td) / "a.jsonl"
        b = Path(td) / "b.jsonl"
        row = {
            "schema": 1,
            "frame": 0,
            "chars": [{
                "id": 1,
                "pos": [10.0, 0.0, 0.0],
                "life": 1000,
            }],
            "projectiles": [],
        }
        a.write_text(json.dumps(row) + "\n", encoding="utf-8")
        b.write_text(json.dumps(row) + "\n", encoding="utf-8")
        aa = load_jsonl(a)
        bb = load_jsonl(b)
        diffs: list[str] = []
        compare(aa, bb, "trace", diffs, 1e-4, 20)
        assert diffs == []

        changed = json.loads(json.dumps(row))
        changed["chars"][0]["life"] = 999
        b.write_text(json.dumps(changed) + "\n", encoding="utf-8")
        bb = load_jsonl(b)
        compare(aa, bb, "trace", diffs, 1e-4, 20)
        assert any("life" in d for d in diffs)

        metadata_a = {"tick": 10, "rand_seed": 123, "life": 1000}
        metadata_b = {"tick": 99, "rand_seed": 456, "life": 1000}
        metadata_diffs: list[str] = []
        compare(
            metadata_a,
            metadata_b,
            "frame",
            metadata_diffs,
            1e-4,
            20,
            DEFAULT_IGNORED_FIELDS,
        )
        assert metadata_diffs == []

def test_saturn_emitter_contract() -> None:
    source = (
        ROOT / "tools" / "ikemen_oracle" / "saturn_trace.cpp"
    ).read_text(encoding="utf-8")
    for field in [
        "\\\"state_no\\\"",
        "\\\"state_time\\\"",
        "\\\"state_type\\\"",
        "\\\"move_type\\\"",
        "\\\"anim_elem\\\"",
        "\\\"targets\\\"",
        "\\\"hitdef_targets\\\"",
        "\\\"projectiles\\\"",
    ]:
        assert field in source

def test_upstream_build_env() -> None:
    env: dict = {}
    apply_upstream_build_env(env)
    assert env["GOEXPERIMENT"] == "arenas"
    assert env["CGO_ENABLED"] == "1"

    # The repo Makefile exports the SH-2 cross compiler; cgo must not see it.
    env = {"CC": "sh2eb-elf-gcc", "CFLAGS": "-m2", "LDFLAGS": "-T x.ld"}
    apply_upstream_build_env(env)
    assert env["CC"] == "gcc" and env["CXX"] == "g++"
    assert "CFLAGS" not in env and "LDFLAGS" not in env

    # An existing experiment list is extended, never clobbered or duplicated.
    env = {"GOEXPERIMENT": "rangefunc,arenas"}
    apply_upstream_build_env(env)
    assert env["GOEXPERIMENT"] == "rangefunc,arenas"
    env = {"GOEXPERIMENT": "rangefunc"}
    apply_upstream_build_env(env)
    assert env["GOEXPERIMENT"] == "rangefunc,arenas"

    # With no PATH at all nothing can be found, and every gap is reported.
    problems = missing_prerequisites({"PATH": ""})
    assert any("go toolchain" in p for p in problems)
    assert any("pkg-config" in p for p in problems)

def test_stage_screenpack_never_overwrites() -> None:
    with tempfile.TemporaryDirectory() as td:
        engine = Path(td) / "Ikemen-GO"
        pack = Path(td) / "Ikemen-GO-Screenpack"
        (engine / "data").mkdir(parents=True)
        (engine / "data" / "common.cmd").write_text("engine", encoding="utf-8")
        (pack / "data" / "ikemen1").mkdir(parents=True)
        (pack / "data" / "fight.def").write_text("motif", encoding="utf-8")
        (pack / "data" / "common.cmd").write_text("pack", encoding="utf-8")
        (pack / "data" / "ikemen1" / "system.def").write_text("s", encoding="utf-8")
        (pack / "README.md").write_text("ignored", encoding="utf-8")

        assert stage_screenpack(engine, pack) == 2
        assert (engine / "data" / "fight.def").read_text(encoding="utf-8") == "motif"
        assert (engine / "data" / "ikemen1" / "system.def").is_file()
        # the engine's own file wins, and a second pass is a no-op
        assert (engine / "data" / "common.cmd").read_text(encoding="utf-8") == "engine"
        assert not (engine / "README.md").exists()
        assert stage_screenpack(engine, pack) == 0

def test_saturn_trace_shares_the_console_frame_step() -> None:
    trace = (
        ROOT / "tools" / "ikemen_oracle" / "saturn_trace.cpp"
    ).read_text(encoding="utf-8")
    main_c = (
        ROOT / "examples" / "ikemen_saturn" / "main.c"
    ).read_text(encoding="utf-8")
    runner = (
        ROOT / "tools" / "ikemen_oracle" / "run_saturn.py"
    ).read_text(encoding="utf-8")
    # The oracle must measure the code the console runs, not a look-alike loop.
    assert "ik_frame_step(" in trace and "ik_frame_step(" in main_c
    assert "ikemen_frame.c" in runner
    assert "ik_command_update(" not in trace

def test_hook_matches_upstream_symbols() -> None:
    hook = (
        ROOT / "tools" / "ikemen_oracle" / "oracle_hook.go"
    ).read_text(encoding="utf-8")
    required = [
        "c.ss.no",
        "c.ss.time",
        "c.ss.stateType",
        "c.ss.moveType",
        "c.anim.curelem",
        "c.anim.curtime",
        "c.ghv.chainId()",
        "p.ownerId",
        "p.curmisstime",
        "s.randseed",
        "s.roundState()",
    ]
    for symbol in required:
        assert symbol in hook

def test_per_field_tolerance_and_fight_split() -> None:
    # A callable tolerance lets Saturn's Q8.8 drift pass on pos/vel only.
    a = {"chars": [{"pos": [1.0, 0.0], "vel": [2.04, 0.0], "life": 1000.0}]}
    b = {"chars": [{"pos": [1.4, 0.0], "vel": [2.03, 0.0], "life": 1000.5}]}
    def eps(path: str) -> float:
        if ".pos[" in path:
            return 0.5
        if ".vel[" in path:
            return 0.05
        return 1e-4
    diffs: list[str] = []
    compare(a, b, "t", diffs, eps, 20)
    assert len(diffs) == 1 and "life" in diffs[0], diffs

    # The fight runtime is split by responsibility; the oracle builds them all.
    root = Path(__file__).resolve().parents[2]
    parts = sorted(
        (root / "examples" / "ikemen_saturn").glob("ikemen_fight*.c"))
    assert len(parts) >= 15
    runner = (root / "tools" / "ikemen_oracle" / "run_saturn.py").read_text(
        encoding="utf-8")
    assert 'glob("ikemen_fight*.c")' in runner
    for part in parts:
        lines = len(part.read_text(encoding="utf-8").splitlines())
        assert lines < 650, (part.name, lines)

def main() -> int:
    test_install_roundtrip()
    test_input_timeline()
    test_scenario_and_command()
    test_trace_diff()
    test_saturn_emitter_contract()
    test_upstream_build_env()
    test_stage_screenpack_never_overwrites()
    test_saturn_trace_shares_the_console_frame_step()
    test_hook_matches_upstream_symbols()
    test_per_field_tolerance_and_fight_split()
    print("ikemen oracle tools: OK")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())


def test_suite_flags_vacuous_scenarios() -> None:
    from tools.ikemen_oracle.suite import check_expect

    def row(p2_state: int, p2_life: int) -> str:
        chars = [
            {"state_no": 200, "life": 1000},
            {"state_no": p2_state, "life": p2_life},
        ]
        return json.dumps({"schema": 1, "chars": chars})

    with tempfile.TemporaryDirectory() as tmp:
        base = Path(tmp)
        scenario = base / "s.json"
        scenario.write_text(json.dumps({
            "expect": {"p1_states": [200], "p2_life_drops": True},
        }), encoding="utf-8")
        trace = base / "t.jsonl"

        trace.write_text(row(0, 1000) + "\n" + row(0, 1000) + "\n",
                         encoding="utf-8")
        problems = check_expect(scenario, trace)
        assert any("never dropped" in p for p in problems)

        trace.write_text(row(0, 1000) + "\n" + row(5000, 977) + "\n",
                         encoding="utf-8")
        assert check_expect(scenario, trace) == []

        scenario.write_text(json.dumps({"expect": {"p2_states": [5000]}}),
                            encoding="utf-8")
        trace.write_text(row(0, 1000) + "\n", encoding="utf-8")
        assert any("5000" in p for p in check_expect(scenario, trace))


def test_timeline_side_keeps_other_players_script() -> None:
    from tools.ikemen_oracle.inputs import setup_string

    scenario = {"frames": 6, "inputs": [
        {"from": 0, "to": 5, "p1": ["forward"]},
        {"from": 2, "to": 3, "p2": ["a"]},
    ]}
    timeline = build_timeline(scenario)
    assert [p1 for p1, _ in timeline] == [1] * 6
    assert [p2 for _, p2 in timeline] == [0, 0, 16, 16, 0, 0]
    # An event that names both sides still sets both.
    scenario["inputs"].append({"frame": 2, "p1": [], "p2": []})
    assert build_timeline(scenario)[2] == (0, 0)

    assert setup_string({}) == ""
    assert setup_string({"setup": {"p2_life": 1, "p1_power": 1000}}) == \
        "p1_power=1000,p2_life=1"
    try:
        setup_string({"setup": {"p3_life": 1}})
    except ValueError:
        pass
    else:
        raise AssertionError("unknown setup key accepted")


def test_generated_scenarios_are_current() -> None:
    from tools.ikemen_oracle import gen_scenarios

    names = set()
    for name, scenario in gen_scenarios.matrix():
        assert name not in names, f"duplicate scenario {name}"
        names.add(name)
        assert scenario["expect"], f"{name} has no expectation"
        path = gen_scenarios.OUT / f"{name}.json"
        assert path.is_file(), f"{name}: run gen_scenarios.py"
        assert json.loads(path.read_text(encoding="utf-8")) == scenario, \
            f"{name}.json is stale: run gen_scenarios.py"
        # Every input range must fit the scenario.
        build_timeline(scenario)
