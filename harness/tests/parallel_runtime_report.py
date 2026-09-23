#!/usr/bin/env python3
"""Validate and summarize raw parallel_runtime telemetry CSVs."""
from __future__ import annotations

import argparse
import csv
import json
import statistics
from pathlib import Path


def percentile(values: list[int], fraction: float) -> float:
    ordered = sorted(values)
    if not ordered:
        raise ValueError("empty sample")
    position = (len(ordered) - 1) * fraction
    low = int(position)
    high = min(low + 1, len(ordered) - 1)
    return ordered[low] + (ordered[high] - ordered[low]) * (position - low)


def summarize(values: list[int]) -> dict[str, float | int]:
    return {
        "samples": len(values),
        "median": statistics.median(values),
        "p95": percentile(values, 0.95),
        "max": max(values),
    }


def load_input(spec: str) -> tuple[dict, list[dict[str, str]]]:
    mode, objects, faces, filename = spec.split(":", 3)
    path = Path(filename)
    with path.open(newline="", encoding="utf-8") as stream:
        rows = list(csv.DictReader(stream))
    if not rows:
        raise ValueError(f"no samples in {path}")
    return {"mode": mode.upper(), "objects": int(objects),
            "faces_per_object": int(faces), "path": str(path)}, rows


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--out", required=True)
    parser.add_argument("--input", action="append", required=True,
                        help="MODE:OBJECTS:FACES:telemetry.csv")
    parser.add_argument("--normal-input", action="append", default=[],
                        help="MODE,OBJECTS,FACES,BOOT_FRAMES,CYCLE_CSV,INSTRUCTION_CSV")
    args = parser.parse_args()
    summaries = []
    all_rows = []
    for spec in args.input:
        meta, rows = load_input(spec)
        mode = meta["mode"]
        for row in rows:
            if int(row["mode"]) != {"MASTER": 0, "SLAVE": 1, "AUTO": 2}[mode]:
                raise ValueError(f"mode mismatch in {meta['path']}")
            if int(row["geometry_match"]) != 1:
                raise ValueError(f"geometry baseline mismatch in {meta['path']} frame {row['frame']}")
            if int(row["source_faces"]) != meta["objects"] * meta["faces_per_object"]:
                raise ValueError(f"source face count mismatch in {meta['path']} frame {row['frame']}")
            if int(row["prepared_faces"]) > int(row["source_faces"]):
                raise ValueError(f"prepared faces exceed input in {meta['path']} frame {row['frame']}")
            for case in range(4):
                if int(row[f"micro{case}_output_match"]) != 1:
                    raise ValueError(f"microbenchmark result mismatch: {meta['path']} case {case}")
                if mode == "MASTER" and int(row[f"micro{case}_slave_tasks"]) != 0:
                    raise ValueError(f"MASTER dispatched a microbenchmark task to Slave: {meta['path']}")
                if mode in {"SLAVE", "AUTO"}:
                    if int(row[f"micro{case}_slave_tasks"]) == 0:
                        raise ValueError(
                            f"{mode} did not dispatch {meta['path']} case {case} to Slave")
                    if int(row[f"micro{case}_worker_frt_delta"]) == 0:
                        raise ValueError(
                            f"{mode} has no valid Slave-local FRT sample in {meta['path']} case {case}")
            all_rows.append((meta, row))

        geometry_direct = [int(row["direct_total_ticks"]) for row in rows]
        geometry_async = [int(row["async_total_ticks"]) for row in rows]
        summary = dict(meta)
        summary["frames"] = len(rows)
        summary["direct_prepare_merge_ticks"] = summarize(geometry_direct)
        summary["async_submit_overlap_wait_merge_release_ticks"] = summarize(geometry_async)
        summary["median_async_minus_direct_ticks"] = (
            statistics.median(geometry_async) - statistics.median(geometry_direct))
        summary["microbench"] = []
        for case in range(4):
            summary["microbench"].append({
                "payload_bytes": int(rows[0][f"micro{case}_payload_bytes"]),
                "iterations": int(rows[0][f"micro{case}_iterations"]),
                "direct_ticks": summarize([int(row[f"micro{case}_direct_ticks"]) for row in rows]),
                "worker_frt_ticks": summarize([int(row[f"micro{case}_worker_frt_delta"]) for row in rows]),
                "submit_call_ticks": summarize([int(row[f"micro{case}_submit_ticks"]) for row in rows]),
                "master_completion_ticks": summarize([int(row[f"micro{case}_completion_ticks"]) for row in rows]),
                "pipeline_ticks": summarize([int(row[f"micro{case}_pipeline_ticks"]) for row in rows]),
                "worker_local_frt_nonzero_frames": sum(
                    int(row[f"micro{case}_worker_frt_delta"]) != 0 for row in rows),
                "direct_callback_nonzero_frames": sum(
                    int(row[f"micro{case}_direct_ticks"]) != 0 for row in rows),
                "slave_dispatched_frames": sum(int(row[f"micro{case}_slave_tasks"]) != 0 for row in rows),
            })
        summaries.append(summary)

    normal_runs = []
    for spec in args.normal_input:
        mode, objects, faces, boot_frames, cycles_path, instructions_path = spec.split(",", 5)
        with Path(cycles_path).open(newline="", encoding="utf-8") as stream:
            cycle_rows = list(csv.DictReader(stream))
        with Path(instructions_path).open(newline="", encoding="utf-8") as stream:
            instruction_rows = list(csv.DictReader(stream))
        boot = int(boot_frames)
        cycles = [int(row["master_sh2_cycles"]) for row in cycle_rows[boot:]]
        instructions = instruction_rows[boot:]
        if not cycles or not instructions:
            raise ValueError(f"no post-boot samples for {mode}/{objects}x{faces}")
        normal_runs.append({
            "mode": mode.upper(), "objects": int(objects),
            "faces_per_object": int(faces), "frames": len(cycles),
            "master_cycles": summarize(cycles),
            "master_instructions": sum(int(row["master_sh2_instructions"]) for row in instructions),
            "slave_instructions": sum(int(row["slave_sh2_instructions"]) for row in instructions),
            "slave_frames": sum(int(row["slave_sh2_instructions"]) != 0 for row in instructions),
            "cycle_csv": cycles_path, "instruction_csv": instructions_path,
        })

    payload = {
        "format_version": 1,
        "timer": "Master-local 16-bit FRT intervals; no cross-CPU timestamp subtraction",
        "input_files": [meta["path"] for meta, _ in all_rows],
        "workloads": summaries,
        "uninstrumented_frame_runs": normal_runs,
        "worker_frt_status": (
            "advances in at least one task sample" if any(
                item["worker_local_frt_nonzero_frames"]
                for workload in summaries for item in workload["microbench"])
            else "all worker-local FRT deltas are zero; do not infer worker execution duration"
        ),
        "crossover": "none observed" if not any(
            item["median_async_minus_direct_ticks"] < 0 for item in summaries)
            else "candidate workload(s) have lower asynchronous median; inspect full samples and frame cycles",
    }
    out = Path(args.out)
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(payload, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
