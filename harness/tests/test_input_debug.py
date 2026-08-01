r"""test_input_debug.py — assertions over two probe.json runs of the
input_debug example: one with no controller input ("baseline") and one with
a scheduled A-button press ("scenario").

GPL-3.0 (see harness/LICENSE / harness/README.md for why this directory is
licensed separately from the rest of the repo).

Scope note (read before extending this file): this test verifies that the
probe's --pad-button/--pad-press-at/--pad-release-at scheduling actually
reaches Ymir's SMPC peripheral report layer — i.e. the injection plumbing
itself works. It deliberately does NOT assert that input_debug's on-screen
HUD digits change in response (a framebuffer diff), because investigation
while adding this test found that the injected program's main loop only
completes a small, fixed number of iterations under the probe's
frame-stepping before stalling — regardless of --frames budget or whether a
pad is connected. That's a pre-existing gap in the probe's boot-injection
fidelity (see harness/README.md's discussion of direct injection bypassing
BIOS-driven boot), not specific to input_debug or to the pad-injection code
added here, and chasing it is out of scope for this test. Once that stall is
fixed, a framebuffer-diff assertion could be added the same way
test_rbg0_ground.py's tests were — see this file's git history for the
differential approach that was tried and reverted.

Generate the two probe.json files with (requires a Saturn BIOS/IPL image).
Note -Frames 300: investigation while adding this test found input_debug's
first SMPC peripheral poll doesn't happen until deep into the run under the
probe's boot-injection timing (see the scope note above) — the default
-Frames 60 never reaches a single poll, so observed_held_reports would
always read 0 regardless of whether pad injection works. -PadReleaseAt 10 is
deliberately tiny relative to -Frames 300: only a handful of polls ever
happen in one run, all bunched near wherever the first one lands, not spread
evenly across the frame budget, so a wide press window buys nothing.

    .\harness\run-harness.ps1 input_debug -Bios <path> -Frames 300 `
        -Out harness\build\probe_input_debug_baseline.json
    .\harness\run-harness.ps1 input_debug -Bios <path> -Frames 300 `
        -PadButton A -PadPressAt 0 -PadReleaseAt 10 `
        -Out harness\build\probe_input_debug_scenario.json

Two ways to point this test at the JSON files:
  - LIBSATURN_PROBE_JSON_BASELINE / LIBSATURN_PROBE_JSON_SCENARIO env vars, or
  - --baseline-json / --scenario-json on the command line when run standalone.
Falls back to harness/build/probe_input_debug_{baseline,scenario}.json.
"""

import json
import os
import sys
import unittest


def _default_json_path(env_var, filename):
    env = os.environ.get(env_var)
    if env:
        return env
    here = os.path.dirname(os.path.abspath(__file__))
    return os.path.join(here, "..", "build", filename)


def load_probe(path):
    with open(path, "r", encoding="utf-8") as f:
        return json.load(f)


def assert_program_ran(probe, label):
    """Same boot-injection sanity check as test_rbg0_ground.py: fails loudly
    if pc_after_run landed outside the injected program, since every other
    assertion would otherwise just be reading BIOS defaults."""
    boot = probe.get("boot")
    if boot is None or not boot.get("injected"):
        raise AssertionError(f"{label}: probe.json has no boot.injected=true")
    load_addr = boot["load_addr"]
    bin_end = load_addr + boot["bin_bytes"]
    pc = boot["pc_after_run"]
    if not (load_addr <= pc < bin_end):
        raise AssertionError(
            f"{label}: pc_after_run=0x{pc:08X} is outside the injected program's range "
            f"[0x{load_addr:08X}, 0x{bin_end:08X}) — our code never ran (or crashed/hung "
            f"once the pad was connected)."
        )


class InputDebugProbeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        baseline_path = _default_json_path("LIBSATURN_PROBE_JSON_BASELINE", "probe_input_debug_baseline.json")
        scenario_path = _default_json_path("LIBSATURN_PROBE_JSON_SCENARIO", "probe_input_debug_scenario.json")
        for label, path in (("baseline", baseline_path), ("scenario", scenario_path)):
            if not os.path.exists(path):
                raise unittest.SkipTest(
                    f"{label} probe.json not found at {path} — see this file's module "
                    f"docstring for the two run-harness.ps1 invocations needed (requires a "
                    f"Saturn BIOS/IPL image; see harness/README.md)."
                )
        cls.baseline = load_probe(baseline_path)
        cls.scenario = load_probe(scenario_path)
        # Connecting a pad and installing a report callback must not crash or
        # hang the injected program — verify both runs actually executed our
        # code, not just the scenario run (the pad-connected one).
        assert_program_ran(cls.baseline, "baseline")
        assert_program_ran(cls.scenario, "scenario")

    def test_baseline_has_no_pad_connected(self):
        self.assertFalse(self.baseline["input"]["connected"])
        self.assertEqual(self.baseline["input"]["observed_held_reports"], 0)

    def test_scenario_pad_schedule_recorded(self):
        inp = self.scenario["input"]
        self.assertTrue(inp["connected"])
        self.assertEqual(inp["button"], "A")
        self.assertLess(inp["press_at"], inp["release_at"])

    def test_scheduled_press_reaches_smpc(self):
        """The --pad-button/-PadPressAt/-PadReleaseAt schedule must actually
        cause Ymir's SMPC peripheral report to report the button held at
        least once — this is what proves the CLI plumbing (probe_main.cpp's
        button_from_name() mapping, the report callback, ConnectControlPad())
        works end to end, independent of whether input_debug's own code
        ever polls it (see module docstring)."""
        self.assertGreater(
            self.scenario["input"]["observed_held_reports"], 0,
            "pad-button schedule never reported the button held to SMPC — "
            "check --pad-press-at/--pad-release-at against the frame budget "
            "used to generate this probe.json",
        )


if __name__ == "__main__":
    for flag, env_var in (("--baseline-json", "LIBSATURN_PROBE_JSON_BASELINE"),
                           ("--scenario-json", "LIBSATURN_PROBE_JSON_SCENARIO")):
        if flag in sys.argv:
            i = sys.argv.index(flag)
            if i + 1 < len(sys.argv):
                os.environ[env_var] = sys.argv[i + 1]
                del sys.argv[i:i + 2]
    unittest.main()
