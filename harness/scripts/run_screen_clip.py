#!/usr/bin/env python3
"""Run both emulators and package compact diagnostic evidence, even on failure."""
import os
from pathlib import Path
import subprocess
import sys
import zipfile
from local_changes import capture as capture_local_changes, add_to_zip as add_local_changes

ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/"harness/build/screen_clip_bench"
EXAMPLE="screen_clip_bench"

def run():
    OUT.mkdir(parents=True,exist_ok=True)
    env=dict(os.environ,SAT_SKIN_BLEND_BENCH="0",SAT_STATIC_PROJECT_EXPERIMENT="0",SAT_HIERARCHY_EXPERIMENT="0",SAT_SKIN_CHARACTER_EXPERIMENT="0")
    tools=Path(env.get("LIBSATURN_TOOLCHAIN_ROOT",str(Path.home()/"dev/saturn-tools")))
    env["LIBSATURN_TOOLCHAIN_ROOT"]=str(tools)
    env["PATH"]=str(tools/"bin")+os.pathsep+env.get("PATH","")
    env.setdefault("LIBSATURN_BIOS",str(ROOT/"bios/firmware/mpr-17933.bin"))
    if (ROOT/"harness/build-macos/probe").is_file():
        env.setdefault("LIBSATURN_YMIR_BUILD_DIR",str(ROOT/"harness/build-macos"))
    dump=OUT/"wram.bin"
    env["LIBSATURN_SCREEN_CLIP_BENCH_DUMP"]=str(dump)
    env["LIBSATURN_SCREEN_CLIP_BENCH_MAP"]=str(ROOT/f"build/{EXAMPLE}.map")
    names=("analysis.json","telemetry.bin","probe.json","misaligned.txt","run.log","screen.png","wram.bin")
    for name in names:
        (OUT/name).unlink(missing_ok=True)
    med=OUT.parent/f"{EXAMPLE}-mednafen-return.zip"
    med.unlink(missing_ok=True)
    archive=OUT.parent/f"{EXAMPLE}-return.zip"
    archive.unlink(missing_ok=True)
    status=0
    local_changes=capture_local_changes(ROOT)
    try:
        with (OUT/"run.log").open("w") as log:
            commands=[
                ["git","rev-parse","HEAD"],
                ["sh","build-example.sh",EXAMPLE],
                ["sh","harness/run-harness.sh",EXAMPLE,"--frames","9000","--out",str(OUT/"probe.json"),
                 "--","--sh2-cache","on","--dump-wram-high",str(dump),
                 "--misaligned-report",str(OUT/"misaligned.txt"),"--screenshot",f"8999:{OUT}/screen.png"],
                [sys.executable,"-m","unittest","discover","-s","harness/tests","-p","test_screen_clip_bench.py","-v"]]
            for command in commands:
                print("Running:",*command,flush=True)
                log.write(repr(command)+"\n");log.flush()
                result=subprocess.run(command,cwd=ROOT,env=env,stdout=log,stderr=subprocess.STDOUT)
                if result.returncode:
                    status=1;break
            if not (OUT/"analysis.json").is_file():
                status=1
                log.write("Missing Ymir target analysis\n")
        # Run independently even if the first emulator fails.
        result=subprocess.run([sys.executable,"harness/scripts/sh2_kernel_verify_mednafen.py",
                               "--example",EXAMPLE,"--seconds","240"],cwd=ROOT,env=env)
        status |= int(result.returncode!=0)
    finally:
        with zipfile.ZipFile(archive,"w",zipfile.ZIP_DEFLATED) as z:
            add_local_changes(z,local_changes)
            for name in names[:-1]:
                if (OUT/name).is_file():z.write(OUT/name,"ymir/"+name)
            if med.is_file():
                with zipfile.ZipFile(med) as source:
                    for name in source.namelist():z.writestr("mednafen/"+name,source.read(name))
        print("Return:",archive,flush=True)
    return status

if __name__=="__main__":
    raise SystemExit(run())
