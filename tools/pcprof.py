#!/usr/bin/env python3
"""Attribute harness probe PC samples to symbols, so you can see where an
example's emulated time actually goes.

The probe's --profile-pc writes one master-SH2 program-counter sample per
emulated frame. Feed those samples plus the example's linker map here and it
prints a flat profile.

This exists because "the program runs but barely progresses" is otherwise
almost impossible to diagnose: the symptom (a blank or half-drawn framebuffer)
looks identical whether the cause is a rendering bug, a hang, or code that is
simply hundreds of times slower than it should be.

Usage:
    python tools/pcprof.py <samples.txt> <example.map> [--top N]

The map file is build/<example>.map, produced by the Makefile's -Wl,-Map.
Symbol names are C++-mangled; pass --demangle to run them through
sh2eb-elf-c++filt if it is on PATH.
"""
import re
import subprocess
import sys
from collections import Counter

SYM_RE = re.compile(r"^\s+0x0*([0-9a-fA-F]{8})\s+(\S+)\s*$")


def load_symbols(map_path):
    """Every address/name pair in the map, sorted, so a sample can be placed
    in the last symbol that starts at or before it."""
    syms = []
    with open(map_path, encoding="utf-8", errors="replace") as fh:
        for line in fh:
            m = SYM_RE.match(line)
            if m:
                syms.append((int(m.group(1), 16), m.group(2)))
    syms.sort()
    # Collapse duplicate addresses, keeping the first name seen.
    out = []
    for addr, name in syms:
        if out and out[-1][0] == addr:
            continue
        out.append((addr, name))
    return out


def resolve(syms, pc):
    lo, hi = 0, len(syms)
    while lo < hi:
        mid = (lo + hi) // 2
        if syms[mid][0] <= pc:
            lo = mid + 1
        else:
            hi = mid
    if lo == 0:
        return None
    return syms[lo - 1]


def demangle(names):
    try:
        p = subprocess.run(["sh2eb-elf-c++filt"], input="\n".join(names),
                           capture_output=True, text=True, check=True)
        return dict(zip(names, p.stdout.splitlines()))
    except (OSError, subprocess.CalledProcessError):
        return {}


def main(argv):
    args = [a for a in argv[1:] if not a.startswith("--")]
    flags = [a for a in argv[1:] if a.startswith("--")]
    if len(args) < 2:
        sys.exit(__doc__)
    samples_path, map_path = args[0], args[1]
    top = 20
    for f in flags:
        if f.startswith("--top"):
            top = int(f.split("=", 1)[1]) if "=" in f else 20

    syms = load_symbols(map_path)
    if not syms:
        sys.exit(f"{map_path}: no symbols found -- is this a linker map?")

    counts = Counter()
    total = 0
    unresolved = 0
    for line in open(samples_path, encoding="utf-8"):
        line = line.strip()
        if not line:
            continue
        pc = int(line, 16)
        total += 1
        hit = resolve(syms, pc)
        if hit is None:
            unresolved += 1
            continue
        counts[hit[1]] += 1

    if total == 0:
        sys.exit(f"{samples_path}: no samples")

    names = [n for n, _ in counts.most_common(top)]
    nice = demangle(names) if "--demangle" in flags else {}

    print(f"{total} samples from {samples_path}")
    if unresolved:
        print(f"  ({unresolved} below the first symbol -- likely BIOS or "
              f"pre-injection code)")
    print()
    print(f"{'samples':>8}  {'share':>7}  symbol")
    for name, n in counts.most_common(top):
        print(f"{n:>8}  {100.0 * n / total:>6.1f}%  {nice.get(name, name)}")


if __name__ == "__main__":
    main(sys.argv)
