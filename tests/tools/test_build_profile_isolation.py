#!/usr/bin/env python3
"""Exercise Make's real evaluated build paths without requiring SH-2 toolchain."""
import hashlib
import subprocess
import sys
from pathlib import Path

root = Path(__file__).resolve().parents[2]


def profile(*args: str) -> dict[str, str]:
    output = subprocess.check_output(
        ["make", "-s", "print-build-paths", *args],
        cwd=root, text=True,
    )
    return dict(line.split("=", 1) for line in output.splitlines() if "=" in line)


hello = profile("EXAMPLE=hello_world")
sky_default = profile("EXAMPLE=skybridge_3d")
sky_profile = profile("EXAMPLE=skybridge_3d", "SAT_SKYBRIDGE_VALIDATION=1")
sky_profile_again = profile("EXAMPLE=skybridge_3d", "SAT_SKYBRIDGE_VALIDATION=1")
sky_fault = profile(
    "EXAMPLE=skybridge_3d", "SAT_SKYBRIDGE_VALIDATION=1",
    "SAT_PARALLEL_TEST_FAULT=2",
)
sky_mode = profile("EXAMPLE=skybridge_3d", "SAT_SKYBRIDGE_PARALLEL_MODE=1")
other = profile("EXAMPLE=parallel_runtime", "SAT_PARALLEL_RUNTIME_VALIDATION=1")

assert hello["library"] == sky_default["library"], "default library must be reused across games"
assert hello["library"] != sky_profile["library"], "instrumented library may not share default objects"
assert sky_profile["library"] == other["library"], "generic profiling must not depend on the game"
assert sky_profile["library"] != sky_fault["library"], "fault injection requires distinct lib objects"
assert sky_default["objects"] != sky_profile["objects"], "example objects leaked across profiles"
assert sky_default["artifacts"] != sky_profile["artifacts"], "ROM variants share one output path"
assert sky_default["objects"] != sky_mode["objects"], "game-specific mode does not isolate objects"
assert sky_default["library"] == sky_mode["library"], "game mode incorrectly recompiles generic library"
assert sky_profile == sky_profile_again, "same build configuration changed paths"
assert sky_profile["artifacts"].startswith("build/variants/skybridge_3d/")
assert sky_default["objects"].startswith("build/objects/examples/skybridge_3d/")
assert not any(" " in p for p in sky_default.values()), "build paths must remain shell-safe"
makefile = (root / "Makefile").read_text(encoding="utf-8")
assert "profile-flags:" not in makefile, "phony forcing profiler rebuilds remains"
assert "CFLAGS      := $(BASE_CFLAGS) -I$(GENERATED_DIR)" in makefile, "example missing generated headers"
assert "LIB_CFLAGS := $(BASE_CFLAGS)" in makefile, "library unexpectedly consumes example flags"
assert "cp $(ISO) $(BUILD_DIR)/$(EXAMPLE).iso" in makefile, "variant alias is missing"
key_tool = root / "tools" / "build_variant_key.py"
key_a = subprocess.check_output([sys.executable, str(key_tool), "x", "y"], text=True).strip()
key_b = subprocess.check_output([sys.executable, str(key_tool), "x", "z"], text=True).strip()
assert key_a != key_b and len(key_a) == 16
print("build profile isolation: OK")
