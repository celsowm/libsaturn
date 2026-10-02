"""The runtime source manifest and src/ must describe the same files.

cmake/LibSaturnSources.cmake is the one authoritative runtime inventory.
Adding a source under src/ without listing it (or listing a file that does
not exist) would make the CMake package and the Makefile silently disagree
with the tree, so fail here instead.
"""
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
sys.path.insert(0, str(ROOT / "tools"))
import library_sources  # noqa: E402

lists = library_sources.parse_manifest()
listed = library_sources.library_sources(lists)
startup = library_sources.startup_sources(lists)

assert listed, "manifest lists no library sources"
assert startup, "manifest lists no startup sources"
dupes = sorted({f for f in listed + startup if (listed + startup).count(f) > 1})
assert not dupes, f"sources listed twice: {dupes}"

on_disk = sorted(p.relative_to(ROOT).as_posix()
                 for ext in ("*.c", "*.cpp") for p in (ROOT / "src").rglob(ext))
asm_on_disk = sorted({p.relative_to(ROOT).as_posix()
                      for ext in ("*.s", "*.S")
                      for p in (ROOT / "src").rglob(ext)})

assert sorted(listed) == on_disk, (
    "manifest/src mismatch: only in manifest="
    f"{sorted(set(listed) - set(on_disk))}, only on disk="
    f"{sorted(set(on_disk) - set(listed))}")
assert sorted(startup) == asm_on_disk, (
    f"startup manifest {sorted(startup)} != assembly on disk {asm_on_disk}")

# Each subsystem list only owns files below its own directory.
owners = {
    "LIBSATURN_CORE_SOURCES": "src/core/",
    "LIBSATURN_GRAPHICS_SOURCES": "src/graphics/",
    "LIBSATURN_AUDIO_SOURCES": "src/audio/",
    "LIBSATURN_HAL_SOURCES": "src/hal/",
    "LIBSATURN_INPUT_SOURCES": "src/input/",
    "LIBSATURN_PHYSICS_SOURCES": "src/physics/",
    "LIBSATURN_RESOURCES_SOURCES": "src/resources/",
    "LIBSATURN_STORAGE_SOURCES": "src/storage/",
}
for name, prefix in owners.items():
    for f in lists[name]:
        assert f.startswith(prefix), f"{f} is in {name} but not under {prefix}"

# The CLI the Makefile shells out to agrees with the parser.
cli = subprocess.run(
    [sys.executable, str(ROOT / "tools" / "library_sources.py")],
    capture_output=True, text=True, check=True).stdout.split()
assert cli == listed
cli_startup = subprocess.run(
    [sys.executable, str(ROOT / "tools" / "library_sources.py"), "--startup"],
    capture_output=True, text=True, check=True).stdout.split()
assert cli_startup == startup

print("library source manifest OK:", len(listed), "sources,",
      len(startup), "startup objects")
