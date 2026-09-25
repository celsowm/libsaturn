#!/usr/bin/env python3
"""A plain example build holds a per-example flock; other goals do not."""
import os
import shutil
import subprocess
from pathlib import Path

root = Path(__file__).resolve().parents[2]


def dry_run(*args: str, locked: bool = False) -> str:
    env = dict(os.environ)
    env.pop("LIBSATURN_EXAMPLE_LOCKED", None)
    if locked:
        env["LIBSATURN_EXAMPLE_LOCKED"] = "1"
    return subprocess.check_output(
        ["make", "-n", *args], cwd=root, text=True, env=env,
        stderr=subprocess.STDOUT,
    )


if shutil.which("flock") is None:
    print("example build lock: SKIP (no flock)")
    raise SystemExit(0)

lock = "flock build/locks/hello_world.lock"
assert lock in dry_run("EXAMPLE=hello_world"), "default goal must take the example lock"
assert lock in dry_run("EXAMPLE=hello_world", "all"), "explicit all must take the example lock"
assert "flock build/locks/sega_bg.lock" in dry_run("EXAMPLE=sega_bg"), "lock must be per example"
assert "flock" not in dry_run("EXAMPLE=hello_world", locked=True), "locked run must not re-lock"
assert "flock" not in dry_run("EXAMPLE=hello_world", "print-build-paths"), "only all is locked"
print("example build lock: OK")
