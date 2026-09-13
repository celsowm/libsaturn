#!/usr/bin/env python3
"""Fail the build if a linked Saturn ELF contains static constructors.

Nothing runs them. src/core/crt0.s jumps straight to _main and
src/core/saturn.ld has no .init_array/.ctors pass, so any C++ global that
needs dynamic initialization is left holding whatever .bss holds: zero.

That failure mode is close to invisible. A `volatile uint16_t& TVSTAT =
*reinterpret_cast<volatile uint16_t*>(0x25F80004);` at namespace scope is not
constant-initialized -- reinterpret_cast is never a constant expression -- so
the compiler puts a pointer in .bss and emits a constructor to fill it. GCC
constant-folds most such references at -O2, which is exactly what makes this
dangerous: it usually works, so the cases where it does not look like a
hardware or emulator problem instead of a startup problem. The real instance
this check was written for left TVMD and TVSTAT null, which swallowed every
display-mode write and made the VBLANK poll spin its entire 2,000,000
iteration timeout every frame -- the program ran about 85x too slow while
still appearing to work.

Either run the constructors from crt0, or keep this passing.

Usage:
    python tools/check_no_static_ctors.py --elf build/foo.elf [--objdump CMD]
"""
import argparse
import re
import shutil
import subprocess
import sys

# Section names a toolchain may use for "call these before main".
CTOR_SECTIONS = (".init_array", ".ctors", ".preinit_array")

HEADER_RE = re.compile(
    r"^\s*\d+\s+(\S+)\s+([0-9a-fA-F]+)\s+([0-9a-fA-F]+)", re.M)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--elf", required=True)
    ap.add_argument("--objdump", default="sh2eb-elf-objdump")
    args = ap.parse_args()

    objdump = shutil.which(args.objdump)
    if objdump is None:
        # Not fatal: a missing cross-objdump must not break a build that
        # otherwise succeeded, but say so rather than silently passing.
        print(f"[ctors] SKIP {args.elf}: {args.objdump} not on PATH")
        return 0

    try:
        out = subprocess.run([objdump, "-h", args.elf],
                             capture_output=True, text=True,
                             check=True).stdout
    except subprocess.CalledProcessError as exc:
        print(f"[ctors] ERROR reading {args.elf}: {exc}", file=sys.stderr)
        return 1

    offenders = []
    for name, size_hex, _vma in HEADER_RE.findall(out):
        if name in CTOR_SECTIONS:
            size = int(size_hex, 16)
            if size > 0:
                offenders.append((name, size))

    if offenders:
        print(f"[ctors] FAIL {args.elf}", file=sys.stderr)
        for name, size in offenders:
            print(f"  {name}: {size} bytes = {size // 4} static constructor(s) "
                  f"that will never run", file=sys.stderr)
        print("  A namespace-scope global needs dynamic initialization. The "
              "usual cause is a reference or pointer bound to a "
              "reinterpret_cast; make it a macro or an inline accessor so the "
              "address is materialized at each use. See src/hal/vdp2.cpp.",
              file=sys.stderr)
        return 1

    print(f"[ctors] {args.elf}: no static constructors")
    return 0


if __name__ == "__main__":
    sys.exit(main())
