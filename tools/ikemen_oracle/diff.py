#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import math
from pathlib import Path
from typing import Any

DEFAULT_FLOAT_EPS = 1e-4
DEFAULT_IGNORED_FIELDS = {"tick", "rand_seed"}

def load_jsonl(path: Path) -> list[dict]:
    rows: list[dict] = []
    with path.open("r", encoding="utf-8") as f:
        for line_no, line in enumerate(f, 1):
            if not line.strip():
                continue
            row = json.loads(line)
            if row.get("schema") != 1:
                raise SystemExit(
                    f"{path}:{line_no}: unsupported schema "
                    f"{row.get('schema')!r}"
                )
            rows.append(row)
    return rows

def compare(
    expected: Any,
    actual: Any,
    path: str,
    diffs: list[str],
    eps: float,
    limit: int,
    ignored_fields: set[str] | None = None,
) -> None:
    if len(diffs) >= limit:
        return
    if ignored_fields is None:
        ignored_fields = set()
    if isinstance(expected, bool) or isinstance(actual, bool):
        if expected != actual:
            diffs.append(f"{path}: expected {expected!r}, got {actual!r}")
        return
    if isinstance(expected, (int, float)) and isinstance(actual, (int, float)):
        if isinstance(expected, float) or isinstance(actual, float):
            if not math.isclose(
                float(expected), float(actual),
                rel_tol=0.0, abs_tol=eps,
            ):
                diffs.append(
                    f"{path}: expected {expected!r}, got {actual!r}"
                )
        elif expected != actual:
            diffs.append(f"{path}: expected {expected!r}, got {actual!r}")
        return
    if type(expected) is not type(actual):
        diffs.append(
            f"{path}: type {type(expected).__name__} != "
            f"{type(actual).__name__}"
        )
        return
    if isinstance(expected, dict):
        keys = sorted(set(expected) | set(actual))
        for key in keys:
            if key in ignored_fields:
                continue
            if key not in expected:
                diffs.append(f"{path}.{key}: unexpected field")
            elif key not in actual:
                diffs.append(f"{path}.{key}: missing field")
            else:
                compare(
                    expected[key], actual[key],
                    f"{path}.{key}", diffs, eps, limit,
                    ignored_fields,
                )
            if len(diffs) >= limit:
                return
        return
    if isinstance(expected, list):
        if len(expected) != len(actual):
            diffs.append(
                f"{path}: length {len(expected)} != {len(actual)}"
            )
            return
        for index, (a, b) in enumerate(zip(expected, actual)):
            compare(
                a, b, f"{path}[{index}]", diffs, eps, limit,
                ignored_fields,
            )
            if len(diffs) >= limit:
                return
        return
    if expected != actual:
        diffs.append(f"{path}: expected {expected!r}, got {actual!r}")

def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("oracle", type=Path)
    parser.add_argument("candidate", type=Path)
    parser.add_argument("--float-eps", type=float, default=DEFAULT_FLOAT_EPS)
    parser.add_argument("--limit", type=int, default=50)
    parser.add_argument(
        "--strict-metadata",
        action="store_true",
        help="also compare engine-global tick and mutable RNG state",
    )
    args = parser.parse_args()

    oracle = load_jsonl(args.oracle)
    candidate = load_jsonl(args.candidate)
    diffs: list[str] = []

    if len(oracle) != len(candidate):
        diffs.append(
            f"trace length: oracle={len(oracle)} "
            f"candidate={len(candidate)}"
        )

    for i, (expected, actual) in enumerate(zip(oracle, candidate)):
        compare(
            expected, actual, f"frame[{i}]",
            diffs, args.float_eps, args.limit,
            set() if args.strict_metadata else DEFAULT_IGNORED_FIELDS,
        )
        if len(diffs) >= args.limit:
            break

    if diffs:
        print("oracle mismatch")
        for diff in diffs:
            print(f"- {diff}")
        if len(diffs) >= args.limit:
            print(f"... stopped after {args.limit} differences")
        return 1

    print(f"oracle match: {len(oracle)} frames")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
