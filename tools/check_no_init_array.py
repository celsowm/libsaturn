#!/usr/bin/env python3
"""Compatibility entry point for the Saturn static-constructor link guard.

The Makefile historically invokes this script with a positional ELF path,
while the canonical checker is tools/check_no_static_ctors.py and expects
--elf. Keep one implementation of the actual check and translate only the
command-line contract here.
"""

from pathlib import Path
import subprocess
import sys


def main() -> int:
    if len(sys.argv) != 2:
        print(f"usage: {Path(sys.argv[0]).name} <elf>", file=sys.stderr)
        return 2

    checker = Path(__file__).with_name("check_no_static_ctors.py")
    return subprocess.call([sys.executable, str(checker), "--elf", sys.argv[1]])


if __name__ == "__main__":
    raise SystemExit(main())
