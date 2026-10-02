"""The CMake package and the Makefile must build the runtime the same way.

Both builds compile the same sources (cmake/LibSaturnSources.cmake). This
guards the other half of that promise: the machine/ABI and language flags.
The Makefile stays authoritative for the example workflow while the CMake
package is built, so a flag added to one and not the other would produce two
runtimes that differ in ways no test sees (the runtime depends on -O2 constant
folding, see tools/check_no_static_ctors.py).

The CMake side is checked against a compile_commands.json when
LIBSATURN_CMAKE_BUILD_DIR points at a configured build tree
(-DCMAKE_EXPORT_COMPILE_COMMANDS=ON); without it only the Makefile parse runs.
"""
import json
import os
import re
import shlex
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
makefile = (ROOT / "Makefile").read_text(encoding="utf-8")
# Join backslash-continued lines so multi-line assignments parse as one.
makefile = re.sub(r"\\\r?\n\s*", " ", makefile)


def assignment(name):
    match = re.search(rf"^{name}\s*:?=\s*(.*)$", makefile, re.M)
    assert match, f"Makefile has no {name} assignment"
    return match.group(1).split("#")[0].strip()


base_cflags = assignment("BASE_CFLAGS")
lib_cxx_extra = assignment("LIB_CXXFLAGS")
asflags = assignment("ASFLAGS")

# ABI/optimization flags every runtime translation unit gets from the Makefile.
MACHINE = ["-m2", "-mb", "-O2", "-ffreestanding", "-fomit-frame-pointer",
           "-Wall", "-Wextra"]
CXX_ONLY = ["-std=c++20", "-fno-exceptions", "-fno-rtti",
            "-fno-threadsafe-statics", "-fno-use-cxa-atexit"]
DEFINES = ["-DSAT_PROFILE_METRICS=0", "-DSAT_PARALLEL_TEST_FAULT=0"]

for flag in MACHINE:
    assert flag in base_cflags.split(), f"Makefile BASE_CFLAGS lost {flag}"
for flag in CXX_ONLY:
    assert flag in lib_cxx_extra.split(), f"Makefile LIB_CXXFLAGS lost {flag}"
for flag in ("-m2", "-mb"):
    assert flag in asflags.split(), f"Makefile ASFLAGS lost {flag}"

build_dir = os.environ.get("LIBSATURN_CMAKE_BUILD_DIR")
if not build_dir:
    print("cmake flag parity: Makefile flags parsed; "
          "set LIBSATURN_CMAKE_BUILD_DIR to compare with a CMake build")
    sys.exit(0)

database = Path(build_dir) / "compile_commands.json"
assert database.exists(), (
    f"{database} missing: configure with -DCMAKE_EXPORT_COMPILE_COMMANDS=ON")
entries = json.loads(database.read_text(encoding="utf-8"))


def tokens(entry):
    if "arguments" in entry:
        return entry["arguments"]
    return shlex.split(entry["command"], posix=False)


def last_optimization(args):
    levels = [a for a in args if re.fullmatch(r"-O[0-3sgz]?", a)]
    return levels[-1] if levels else None


checked = {"cxx": 0, "c": 0, "asm": 0}
for entry in entries:
    args = tokens(entry)
    source = entry["file"].replace("\\", "/")
    if "/src/" not in source:
        continue  # only runtime sources are under comparison
    if source.endswith(".cpp"):
        kind, required = "cxx", MACHINE + CXX_ONLY + DEFINES
    elif source.endswith(".c"):
        kind, required = "c", MACHINE + DEFINES
    elif source.endswith(".s"):
        kind, required = "asm", ["-m2", "-mb"]
    else:
        continue
    missing = [flag for flag in required if flag not in args]
    assert not missing, f"{source}: CMake build is missing {missing}"
    if kind != "asm":
        assert last_optimization(args) == "-O2", (
            f"{source}: effective optimization is {last_optimization(args)}, "
            "the Makefile builds the runtime with -O2")
    checked[kind] += 1

assert checked["cxx"] > 0 and checked["c"] > 0 and checked["asm"] > 0, (
    f"compile database did not cover every language: {checked}")
print(f"cmake flag parity OK: {checked}")
