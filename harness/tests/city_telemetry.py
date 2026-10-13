"""Reads city_walk's g_city struct out of a Work RAM High dump.

The field list is parsed from examples/city_walk/city_walk.h, so the C struct
and this reader cannot drift: add a field there and it shows up here. The
address comes from the linker map (`_g_city`) via harness.scripts.telemetry;
no offsets are hard-coded.

    python harness/tests/city_telemetry.py build/city_walk.map harness/build/city_wramh.bin
"""

import re
import sys
from pathlib import Path

from harness.scripts import telemetry as tel

REPO = Path(__file__).resolve().parents[2]
HEADER = REPO / "examples" / "city_walk" / "city_walk.h"
MAGIC = 0x43495459  # "CITY"

STATES = {0: "BOOT", 1: "LOADING", 2: "RUNNING", 3: "NO_CART", 4: "CART_TOO_SMALL",
          5: "LOAD_FAILED"}


def field_names():
    """uint32_t names inside `typedef struct city_telemetry { ... }`, in order."""
    text = HEADER.read_text(encoding="utf-8")
    body = re.search(r"typedef struct city_telemetry \{(.*?)\} city_telemetry_t;", text, re.S)
    if not body:
        raise RuntimeError("city_telemetry_t not found in city_walk.h")
    names = []
    for line in body.group(1).splitlines():
        line = re.sub(r"/\*.*?\*/", "", line).split("//")[0].strip()
        match = re.match(r"uint32_t\s+(.+);", line)
        if match:
            names += [n.strip() for n in match.group(1).split(",")]
    return names


def read(map_path, dump_path):
    names = field_names()
    values = tel.load_dump(
        str(dump_path), str(map_path), "_g_city",
        ">%dI" % len(names), 4 * len(names))
    return dict(zip(names, values))


def signed(value):
    return value - (1 << 32) if value & 0x80000000 else value


def main(argv):
    data = read(argv[1], argv[2])
    if data["magic"] != MAGIC:
        print("g_city is uninitialised: magic=%08X" % data["magic"])
        return 1
    for name, value in data.items():
        extra = ""
        if name == "state":
            extra = "  " + STATES.get(value, "?")
        if name == "status":
            extra = "  (%d)" % signed(value)
        print("%-26s %d (0x%X)%s" % (name, value, value, extra))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
