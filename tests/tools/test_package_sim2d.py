"""LibSaturn::Sim2D must stay a closed set of hardware-free runtime sources.

cmake/LibSaturnSim2D.cmake lists the files a consumer compiles for host simulation. They are
installed as they are, so:
  - every listed file exists and every .cpp is also a runtime source in the manifest;
  - every private "src/..." include of a listed file is itself listed (a missing one would
    only show up in a consumer's build);
  - no listed file reaches for hardware (no registers, no VDP/SCU/SMPC headers).
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
sys.path.insert(0, str(ROOT / "tools"))
import library_sources  # noqa: E402

text = re.sub(r"#[^\n]*", "", (ROOT / "cmake" / "LibSaturnSim2D.cmake").read_text(encoding="utf-8"))
match = re.search(r"set\(\s*LIBSATURN_SIM2D_FILES\b([^)]*)\)", text)
assert match, "LIBSATURN_SIM2D_FILES not found"
files = match.group(1).split()
assert len(files) == len(set(files)), "a file is listed twice"

manifest = set(library_sources.library_sources(library_sources.parse_manifest()))
INCLUDE = re.compile(r'^\s*#\s*include\s+"(src/[^"]+)"', re.M)
HARDWARE = re.compile(r'#\s*include\s+"saturn/(vdp1|vdp2|vdp2_\w+|scu\w*|smpc|scsp\w*|irq|dma|abus|cd\w*|uart)\.h"')

for rel in files:
    path = ROOT / rel
    assert path.is_file(), f"{rel} does not exist"
    if rel.endswith(".cpp"):
        assert rel in manifest, f"{rel} is not a runtime source in cmake/LibSaturnSources.cmake"
    source = path.read_text(encoding="utf-8")
    for inc in INCLUDE.findall(source):
        assert inc in files, f"{rel} includes {inc}, which is not in LIBSATURN_SIM2D_FILES"
    assert not HARDWARE.search(source), f"{rel} includes a hardware header"

print(f"Sim2D file list: OK ({len(files)} files)")
