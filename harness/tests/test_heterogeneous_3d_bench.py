"""Decode and validate the guest report from either emulator."""

import argparse
import json
import struct
from pathlib import Path

from harness.scripts.telemetry import find_symbol

SYMBOL = "_g_heterogeneous_3d_bench"
MODES = ("master_c", "master_mac", "dual_c", "dual_mac", "dsp_ports",
         "dsp_stream", "master_dsp", "triple_eighth", "triple_quarter",
         "triple_three_eighths", "dual_slave_weighted", "triple_slave_weighted")
SIZES = tuple(16 << i for i in range(8))
HEADER = ("magic status errors completed frc_hz refresh_hz scene_count texture_bytes "
          "live_frames audio_refills audio_underruns audio_consumed audio_playing").split()
KERNEL = "total best worst master slave dsp dispatch join ports dsp_vertices slave_vertices".split()
SCENE = "mode frames swaps fields ready worst geometry master slave dsp dma_wait submitted culled latency late digest active dma_elapsed".split()
SCENE_COUNT = 5
SIZE = 4 * (len(HEADER) + len(KERNEL) * len(SIZES) * 10 + SCENE_COUNT * len(SCENE))


def decode(raw):
    if len(raw) != SIZE:
        raise ValueError(f"Expected {SIZE} report bytes, got {len(raw)}")
    words = iter(struct.unpack(f">{SIZE//4}I", raw))

    def row(fields):
        return {field: next(words) for field in fields}

    report = row(HEADER)
    report["sweep"] = {
        str(count): {mode: row(KERNEL) for mode in MODES[:10]} for count in SIZES
    }
    report["scenes"] = [row(SCENE) for _ in range(SCENE_COUNT)]
    for scene in report["scenes"]:
        scene["backend"] = MODES[scene["mode"]]
        if scene["fields"]:
            scene["swaps_per_second"] = scene["swaps"] * report["refresh_hz"] / scene["fields"]
    return report


def validate(report, require_stress=False):
    def require(condition, message):
        if not condition:
            raise ValueError(message)

    for key, value in (("magic", 0x48334431), ("status", 0), ("errors", 0),
                       ("completed", len(SIZES)), ("scene_count", SCENE_COUNT)):
        require(report[key] == value, f"{key} is {report[key]}, expected {value}")
    require(report["frc_hz"] > 0 and report["refresh_hz"] in (50, 60), "Invalid clock")
    if require_stress:
        require(report["live_frames"] >= 240, "Expected at least one sustained dual/triple replay")
        require(report["audio_playing"] == 1 and report["audio_refills"] > 8
                and report["audio_consumed"] > 32768, "PCM stream did not sustain playback")
        require(report["audio_underruns"] == 0, "PCM stream underruns during replay")
    for count, rows in report["sweep"].items():
        for name, row in rows.items():
            require(0 < row["best"] <= row["total"] / 16 <= row["worst"],
                    f"Invalid FRT samples for {count}/{name}")
            if int(count) >= 64 and name.startswith("triple"):
                require(row["dsp_vertices"] > 0 and row["slave_vertices"] > 0 and
                        row["master"] > 0 and row["slave"] > 0 and row["dsp"] > 0,
                        f"Expected three active processors for {count}/{name}")
    for index, scene in enumerate(report["scenes"]):
        require(scene["frames"] == scene["swaps"] == 120 and scene["fields"] >= 120,
                f"Missing frame or swap in scene {index+1}")
        require((scene["dsp"] > 0) == (index in (2, 4)),
                f"Unexpected DSP activity in scene {index+1}")
        require(scene["dma_elapsed"] > 0, f"Missing DMA timing in scene {index+1}")
        require(scene["submitted"] > 0 and scene["culled"] > 0 and scene["latency"] > 0,
                f"Incomplete scene {index+1} counters")
        for key in ("digest", "submitted", "culled"):
            require(scene[key] == report["scenes"][0][key],
                    f"Scene {index+1} differs in {key}")


def read_report(ram, map_path):
    address = find_symbol(map_path, SYMBOL)
    return decode(ram[address:address+SIZE])


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("ram", type=Path)
    parser.add_argument("map", type=Path)
    args = parser.parse_args()
    report = read_report(args.ram.read_bytes(), args.map)
    print(json.dumps(report, indent=2))
    validate(report)
