#!/usr/bin/env python3
"""Provenance of a C3 Tekken A/B run: SDK defines, archive, emitter disassembly, linked archive.

For each mode directory of a run_vdp1_emit_tekken_v4.py run this records the
compile definition of vdp1.cpp, the installed archive hash, the three emitter
symbols, their SH-2 disassembly with store-width counts, and which archive the
game's linker map pulled vdp1.cpp.obj from. Writes <run>/provenance.json and
<run>/<mode>/emitters.s. Reads build outputs only.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess

EMITTERS = ("push_projected_program", "push_projected_sprite", "push_distorted_sprite")


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def tool(toolchain, name):
    return str(Path(toolchain) / "bin" / ("sh2eb-elf-" + name))


def inspect(mode_dir, toolchain, game_map):
    archive = next((mode_dir / "sdk").rglob("libsaturn.a"))
    ninja = (mode_dir / "sdk-build" / "build.ninja").read_text()
    # The compile line of the VDP1 HAL object carries the experiment define.
    block = next(b for b in ninja.split("\nbuild ") if "src/hal/vdp1/vdp1.cpp.obj:" in b.split("\n", 1)[0])
    define = re.findall(r"-DSAT_VDP1_EMIT_EXPERIMENT=\d+", block)
    obj = next((mode_dir / "sdk-build").rglob("vdp1.cpp.obj"))
    nm = subprocess.check_output([tool(toolchain, "nm"), "-C", "-S", "--defined-only", str(obj)], text=True)
    symbols = {e: [l for l in nm.splitlines() if "vdp1::" + e + "(" in l] for e in EMITTERS}
    asm = subprocess.check_output([tool(toolchain, "objdump"), "-d", "-C", "--no-show-raw-insn", str(obj)], text=True)
    functions = {}
    for chunk in re.split(r"\n(?=[0-9a-f]+ <)", asm):
        head = chunk.split("\n", 1)[0]
        hit = next((e for e in EMITTERS if "vdp1::" + e + "(" in head), None)
        if not hit:
            continue
        body = chunk.split("\n")[1:]
        stores = lambda w: sum(1 for l in body if re.search(r"\tmov\." + w + r"\t[^,]+,@", l))
        functions[hit] = dict(instructions=sum(1 for l in body if re.match(r"\s+[0-9a-f]+:", l)),
                              stores_w=stores("w"), stores_l=stores("l"))
    (mode_dir / "emitters.s").write_text("\n\n".join(c for c in re.split(r"\n(?=[0-9a-f]+ <)", asm)
                                                    if any("vdp1::" + e + "(" in c.split("\n", 1)[0] for e in EMITTERS)) + "\n")
    linked = None
    if game_map and game_map.is_file():
        text = game_map.read_text(errors="replace")
        hits = sorted(set(re.findall(r"(\S*libsaturn\.a)\((?:[^)]*/)?vdp1\.cpp\.obj\)", text)))
        linked = dict(archives=hits, matches_sdk=bool(hits) and all(Path(h).resolve() == archive.resolve() for h in hits))
    return dict(archive=str(archive), archive_sha256=sha(archive), object_sha256=sha(obj),
                compile_define=define, emitter_symbols=symbols, emitters=functions, linked_from=linked)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("run", type=Path, help="harness/build/vdp1-tekken-v4-* directory")
    p.add_argument("--toolchain", type=Path, default=Path.home() / "dev/saturn-tools")
    p.add_argument("--game-build-root", type=Path, help="where the run put its game builds (for the linker maps)")
    a = p.parse_args()
    run = a.run.resolve()
    result = {}
    for mode in ("control", "hybrid"):
        game = a.game_build_root / f"{run.name}-{mode}" if a.game_build_root else run / mode / "game"
        result[mode] = inspect(run / mode, a.toolchain, game / "tekken3_saturn.elf.map")
    gcc = subprocess.check_output([tool(a.toolchain, "gcc"), "--version"], text=True).splitlines()[0]
    result["compiler"] = gcc
    result["checks"] = dict(
        control_define_absent=not result["control"]["compile_define"],
        hybrid_define_3=result["hybrid"]["compile_define"] == ["-DSAT_VDP1_EMIT_EXPERIMENT=3"],
        archives_differ=result["control"]["archive_sha256"] != result["hybrid"]["archive_sha256"],
        games_link_own_sdk=all((result[m]["linked_from"] or {}).get("matches_sdk") for m in ("control", "hybrid")))
    (run / "provenance.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(dict(checks=result["checks"], emitters={m: result[m]["emitters"] for m in ("control", "hybrid")}), indent=2))


if __name__ == "__main__":
    main()
