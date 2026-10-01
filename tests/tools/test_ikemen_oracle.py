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
from tools.ikemen_oracle.run import build_command, load_scenario

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

        hook = Path(td) / "oracle_hook.go"
        hook.write_text("package main\n", encoding="utf-8")

        install(root, hook)
        patched = system_go.read_text(encoding="utf-8")
        assert patched.count(BEGIN_MARKER) == 2
        assert patched.count(END_MARKER) == 2
        assert (src / "libsaturn_oracle.go").is_file()

        install(root, hook)
        patched2 = system_go.read_text(encoding="utf-8")
        assert patched2.count(BEGIN_MARKER) == 2
        assert patched2.count(END_MARKER) == 2

        uninstall(root)
        assert system_go.read_text(encoding="utf-8") == source
        assert not (src / "libsaturn_oracle.go").exists()

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

def main() -> int:
    test_install_roundtrip()
    test_scenario_and_command()
    test_trace_diff()
    test_saturn_emitter_contract()
    test_hook_matches_upstream_symbols()
    print("ikemen oracle tools: OK")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
