#!/usr/bin/env python3
"""Validate Skybridge runtime telemetry and report matched 360-frame profiles."""

import argparse
import csv
import json
import math
from pathlib import Path


HASH_FIELDS = ("game_hash", "animation_hash", "merge_order_hash",
               "scene_command_hash", "scene_command_count",
               "scene_command_capacity")
TICK_FIELDS = (
    "frame_cpu_frt_ticks", "frame_frt_ticks", "geometry_prepare_frt_ticks",
    "geometry_merge_frt_ticks", "task_input_publish_frt_ticks",
    "task_submit_frt_ticks", "task_wait_frt_ticks", "task_completion_frt_ticks",
    "task_release_frt_ticks", "task_master_frt_ticks", "task_slave_frt_ticks",
    "scene_painter_frt_ticks", "scene_emit_frt_ticks",
)
FAULTS = {
    "SubmitReject": 1,
    "WorkerError": 2,
    "TimeoutAbort": 3,
    "AbortFailure": 4,
    "ReleaseFailure": 5,
}


def load_profile(root: Path, name: str, frames: int):
    path = root / f"{name}.json"
    with path.open(encoding="utf-8") as stream:
        data = json.load(stream)
    if not data.get("boot", {}).get("injected"):
        raise AssertionError(f"{name}: ROM was not injected by the probe")
    telemetry = data.get("skybridge_telemetry")
    if not telemetry or telemetry.get("version") != 5:
        raise AssertionError(f"{name}: missing version-5 Skybridge telemetry")
    samples = telemetry.get("samples", [])
    if len(samples) < frames:
        raise AssertionError(f"{name}: got {len(samples)} gameplay samples, need {frames}")
    samples = samples[:frames]
    if any(samples[i]["serial"] + 1 != samples[i + 1]["serial"]
           for i in range(len(samples) - 1)):
        raise AssertionError(f"{name}: telemetry ring dropped or reordered samples")
    return samples


def read_csv_by_frame(path: Path):
    with path.open(newline="", encoding="utf-8") as stream:
        rows = csv.DictReader(stream)
        return {int(row["frame"]): row for row in rows}


def percentile(values, p):
    ordered = sorted(values)
    return ordered[max(0, math.ceil(p * len(ordered)) - 1)] if ordered else 0


def assert_equal_hashes(profiles, names, frames):
    baseline = profiles[names[0]]
    for name in names[1:]:
        other = profiles[name]
        for index in range(frames):
            for field in HASH_FIELDS:
                if baseline[index][field] != other[index][field]:
                    raise AssertionError(
                        f"{name} differs from {names[0]} at gameplay sample {index}: "
                        f"{field} {other[index][field]:08x} != {baseline[index][field]:08x}")


def require_forced_split(name, samples, require_slave):
    split_frames = [row for row in samples
                    if row["visible_gems"] >= 4 and row["slave_items"] > 0]
    if not split_frames:
        raise AssertionError(f"{name}: no frame split a batch of at least four visible gems")
    if require_slave and not any(row["geometry_slave"] > 0 for row in split_frames):
        raise AssertionError(f"{name}: split geometry never dispatched on the Slave")
    if require_slave and not any(row["geometry_submitted"] and row["geometry_completed"]
                                 for row in split_frames):
        raise AssertionError(f"{name}: geometry counters do not show submit+completion")
    if not any(row["animation_submitted"] and row["animation_completed"]
               for row in samples):
        raise AssertionError(f"{name}: animation counters do not show submit+completion")
    if any(row["master_items"] + row["slave_items"] != row["visible_gems"]
           for row in split_frames):
        raise AssertionError(f"{name}: partition item counts do not cover visible gems")


def validate_fault(root: Path, fault: str, frames: int):
    samples = load_profile(root, f"fault_{fault}", frames)
    bit = 1 << FAULTS[fault]
    if not any(row["fault_mask"] & bit for row in samples):
        raise AssertionError(f"{fault}: executor fault hook was not observed")

    if fault == "SubmitReject":
        if not any(row["sync_fallback_items"] > 0 and row["gem_pending"] == 0
                   and row["merge_batches"] >= 2 for row in samples):
            raise AssertionError("SubmitReject: rejected geometry was not prepared synchronously")
    elif fault == "WorkerError":
        if not any(row["geometry_failed"] and row["gem_task_state"] == 4
                   and row["gem_pending"] == 0 and row["sync_fallback_items"] > 0
                   and row["merge_batches"] >= 2 for row in samples):
            raise AssertionError("WorkerError: terminal task did not fall back and merge safely")
    elif fault == "TimeoutAbort":
        if not any(row["timeouts"] and row["gem_task_state"] == 4
                   and row["gem_pending"] == 0 and row["recovery_blocked"] == 0
                   and row["sync_fallback_items"] > 0 and row["merge_batches"] >= 2
                   for row in samples):
            raise AssertionError("TimeoutAbort: aborted task did not fall back and release safely")
    elif fault in ("AbortFailure", "ReleaseFailure"):
        if fault == "AbortFailure":
            trigger = next((i for i, row in enumerate(samples)
                            if row["recovery_blocked"] and row["gem_pending"]
                            and row["gem_task_state"] == 2), None)
            if trigger is None:
                raise AssertionError("AbortFailure: running task was not retained after abort failed")
        else:
            trigger = next((i for i, row in enumerate(samples)
                            if row["recovery_blocked"] and row["gem_pending"]
                            and row["gem_task_state"] == 3
                            and row["merge_batches"] >= 2), None)
            if trigger is None:
                raise AssertionError("ReleaseFailure: terminal task was not pinned after release failed")
        tail = samples[trigger + 1:]
        if any(row["geometry_submitted"] or row["merge_batches"] for row in tail):
            raise AssertionError(f"{fault}: recovery failure allowed a duplicate submit or merge")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, required=True)
    parser.add_argument("--frames", type=int, default=360)
    parser.add_argument("--skip-faults", action="store_true")
    args = parser.parse_args()
    root = args.root

    profiles = {}
    bench_names = [f"bench_{mode}" for mode in ("MASTER", "SLAVE", "AUTO")]
    split_names = [f"split_{mode}" for mode in ("MASTER", "SLAVE", "AUTO")]
    for name in bench_names + split_names:
        profiles[name] = load_profile(root, name, args.frames)
        if any(row["game_course"] != 1 or row["game_paused"]
               for row in profiles[name][120:]):
            raise AssertionError(f"{name}: scripted gameplay did not remain active on course 2")
    assert_equal_hashes(profiles, ["bench_MASTER"] + bench_names[1:] + split_names,
                        args.frames)

    require_forced_split("bench_SLAVE", profiles["bench_SLAVE"], True)
    for mode in ("SLAVE", "AUTO"):
        require_forced_split(f"split_{mode}", profiles[f"split_{mode}"], True)
    require_forced_split("split_MASTER", profiles["split_MASTER"], False)

    if any(row["geometry_slave"] for row in profiles["bench_AUTO"]):
        raise AssertionError("production AUTO benchmark unexpectedly split gem geometry")
    if not any(row["geometry_slave"] for row in profiles["bench_SLAVE"]):
        raise AssertionError("production SLAVE benchmark did not dispatch gem geometry")
    if any(row["geometry_slave"] or row["animation_slave"]
           for row in profiles["bench_MASTER"]):
        raise AssertionError("MASTER benchmark dispatched a task to the Slave")

    frame_csv = root / "frame_times.csv"
    summary = {"frames_per_mode": args.frames, "timer_resolution_ms": 1,
               "timer": {"clock": "Master SH-2 FRT", "prescaler": 128,
                         "counter_bits": 16, "delta_modulus": 65536,
                         "short_interval_rollover_safe": True,
                         "measurement_overhead_upper_bound_ticks": max(
                             row["timer_read_overhead_frt_ticks"]
                             for row in profiles["bench_MASTER"])},
               "policies": {}, "faults": {}}
    with frame_csv.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=(
            "policy", "sample", "emulated_frame", "game_course", "game_pickups",
            "game_x", "game_y", "game_z", "animation_clip", "animation_frame",
            "animation_time", "game_hash", "animation_hash", "merge_order_hash",
            "frame_cpu_ms", "frame_ms", "wait_ms", "visible_gems",
            *TICK_FIELDS, "scene_command_hash", "scene_command_count",
            "scene_command_capacity", "timer_read_overhead_frt_ticks",
            "master_items", "slave_items", "master_faces", "slave_faces",
            "merge_batches", "merged_faces", "animation_submitted",
            "animation_completed", "animation_master", "animation_slave",
            "geometry_submitted", "geometry_completed", "geometry_master",
            "geometry_slave",
            "master_instructions", "slave_instructions", "master_sh2_cycles"))
        writer.writeheader()
        for mode in ("MASTER", "SLAVE", "AUTO"):
            name = f"bench_{mode}"
            samples = profiles[name]
            instruction_rows = read_csv_by_frame(root / f"{name}_instructions.csv")
            cycle_rows = read_csv_by_frame(root / f"{name}_cycles.csv")
            cpu = [row["frame_cpu_ms"] for row in samples]
            frame = [row["frame_ms"] for row in samples]
            waits = [row["wait_ms"] for row in samples]
            sum_master_instructions = 0
            sum_slave_instructions = 0
            sum_cycles = 0
            for index, row in enumerate(samples):
                emulated_frame = row["emulated_frame"]
                inst = instruction_rows.get(emulated_frame)
                cycle = cycle_rows.get(emulated_frame)
                if inst is None or cycle is None:
                    raise AssertionError(f"{name}: profiler lacks frame {emulated_frame}")
                master_inst = int(inst["master_sh2_instructions"])
                slave_inst = int(inst["slave_sh2_instructions"])
                master_cycles = int(cycle["master_sh2_cycles"])
                sum_master_instructions += master_inst
                sum_slave_instructions += slave_inst
                sum_cycles += master_cycles
                writer.writerow({
                    "policy": mode, "sample": index, "emulated_frame": emulated_frame,
                    "game_course": row["game_course"], "game_pickups": row["game_pickups"],
                    "game_x": row["game_x"], "game_y": row["game_y"],
                    "game_z": row["game_z"], "animation_clip": row["animation_clip"],
                    "animation_frame": row["animation_frame"],
                    "animation_time": row["animation_time"],
                    "game_hash": row["game_hash"],
                    "animation_hash": row["animation_hash"],
                    "merge_order_hash": row["merge_order_hash"],
                    "frame_cpu_ms": row["frame_cpu_ms"], "frame_ms": row["frame_ms"],
                    "wait_ms": row["wait_ms"], "visible_gems": row["visible_gems"],
                    **{field: row[field] for field in TICK_FIELDS},
                    "scene_command_hash": row["scene_command_hash"],
                    "scene_command_count": row["scene_command_count"],
                    "scene_command_capacity": row["scene_command_capacity"],
                    "timer_read_overhead_frt_ticks": row[
                        "timer_read_overhead_frt_ticks"],
                    "master_items": row["master_items"], "slave_items": row["slave_items"],
                    "master_faces": row["master_faces"], "slave_faces": row["slave_faces"],
                    "merge_batches": row["merge_batches"], "merged_faces": row["merged_faces"],
                    "animation_submitted": row["animation_submitted"],
                    "animation_completed": row["animation_completed"],
                    "animation_master": row["animation_master"],
                    "animation_slave": row["animation_slave"],
                    "geometry_submitted": row["geometry_submitted"],
                    "geometry_completed": row["geometry_completed"],
                    "geometry_master": row["geometry_master"],
                    "geometry_slave": row["geometry_slave"],
                    "master_instructions": master_inst,
                    "slave_instructions": slave_inst, "master_sh2_cycles": master_cycles})
            summary["policies"][mode] = {
                "frame_cpu_ms_median": percentile(cpu, 0.50),
                "frame_cpu_ms_p95": percentile(cpu, 0.95),
                "frame_cpu_ms_max": max(cpu, default=0),
                "frame_ms_median": percentile(frame, 0.50),
                "frame_ms_p95": percentile(frame, 0.95),
                "frame_ms_max": max(frame, default=0),
                "over_budget_frames": sum(value > 17 for value in frame),
                "wait_ms_total": sum(waits),
                "master_instructions": sum_master_instructions,
                "slave_instructions": sum_slave_instructions,
                "mean_master_sh2_cycles": sum_cycles // args.frames,
                "geometry_slave_frames": sum(bool(row["geometry_slave"]) for row in samples),
                "animation_slave_frames": sum(bool(row["animation_slave"]) for row in samples),
                "timings_frt_ticks": {
                    field: {"median": percentile([row[field] for row in samples], 0.50),
                            "p95": percentile([row[field] for row in samples], 0.95),
                            "max": max((row[field] for row in samples), default=0)}
                    for field in TICK_FIELDS
                },
            }

    if not args.skip_faults:
        for fault in FAULTS:
            validate_fault(root, fault, args.frames)
            summary["faults"][fault] = "PASS"

    summary["interpretation"] = (
        "Guest timer samples are millisecond-quantized. Emulator instructions and cycles "
        "describe work and emulated CPU activity; these measurements alone do not establish "
        "real-hardware FPS or a speedup.")
    (root / "summary.json").write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
    print("Policy  CPU ms median/p95/max  Frame ms median/p95/max  Over budget  "
          "Master/Slave instructions  Geometry Slave frames")
    for mode in ("MASTER", "SLAVE", "AUTO"):
        item = summary["policies"][mode]
        print(f"{mode:6}  {item['frame_cpu_ms_median']:>3}/"
              f"{item['frame_cpu_ms_p95']:>3}/{item['frame_cpu_ms_max']:>3}"
              f"                 {item['frame_ms_median']:>3}/"
              f"{item['frame_ms_p95']:>3}/{item['frame_ms_max']:>3}"
              f"             {item['over_budget_frames']:>3}          "
              f"{item['master_instructions']}/{item['slave_instructions']}"
              f"     {item['geometry_slave_frames']}")
    print(f"PASS: structured parity across normal and forced-split profiles; "
          f"{len(summary['faults'])} injected-fault profiles; outputs in {root}")


if __name__ == "__main__":
    main()
