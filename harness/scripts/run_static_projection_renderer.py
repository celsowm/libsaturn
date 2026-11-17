#!/usr/bin/env python3
"""Matched renderer A/B with a fixed rotated camera, no interactive input."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import signal
import subprocess
import sys
import tempfile
import zipfile

from cube_pipeline import ROOT, decode, symbols, validate_panic
from compare_cube_pipeline import compare as compare_ymir, load_reports
from sh2_kernel_verify_mednafen import elf_text, normalize, work_ram, sha

CASES=("projection-864","texture-384")
OUT=ROOT/"harness/build/static_project_renderer"


def capture_med(case,bios,env,out):
    exe=shutil.which(env.get("MEDNAFEN","mednafen"),path=env["PATH"])
    if not exe:
        raise RuntimeError("Mednafen not on PATH")
    elf=case/"cube_pipeline_bench.elf"
    syms=symbols(elf,"sh2eb-elf-nm")
    address,size=syms["g_cube_bench"]
    panic_address,panic_size=syms["g_sat_last_panic"]
    offset=address-0x06000000
    panic_offset=panic_address-0x06000000
    if not 0<=panic_offset<=0x100000-panic_size:
        raise ValueError("Panic record must be in WRAM-H")
    if not 0<=offset<=0x100000-size:
        raise ValueError("Cube telemetry must be in WRAM-H")
    with tempfile.TemporaryDirectory(prefix="mednafen-",dir=case) as tmp:
        local=dict(env,MEDNAFEN_HOME=tmp)
        command=[exe,"-sound","0","-video.driver","softfb","-ss.region_default","na",
                 "-ss.bios_na_eu",str(bios),"-autosave","1","-ss.input.port2","none"]
        for button in ("up","down","left","right","a","b","c","x","y","z","ls","rs","start"):
            command += ["-ss.input.port1.gamepad."+button,""]
        command.append(str(case/"cube_pipeline_bench.cue"))
        with (case/"mednafen.log").open("w") as log:
            p=subprocess.Popen(command,cwd=ROOT,env=local,stdout=log,stderr=subprocess.STDOUT)
            try:
                try:p.wait(timeout=45)
                except subprocess.TimeoutExpired:
                    p.send_signal(signal.SIGINT);p.wait(timeout=20)
            finally:
                if p.poll() is None:p.kill();p.wait()
        if p.returncode:raise RuntimeError(f"Mednafen exit {p.returncode}")
        states=sorted((Path(tmp)/"mcs").glob("*.mca"))
        if len(states)!=1:raise RuntimeError("Missing or ambiguous Mednafen autosave")
        order,high=normalize(work_ram(states[0]),*elf_text(elf))
        panic=high[panic_offset:panic_offset+panic_size]
        (case/"mednafen-panic.bin").write_bytes(panic)
        validate_panic(panic,panic_size)
        raw=high[offset:offset+size]
        (case/"mednafen-telemetry.bin").write_bytes(raw)
        report=decode(raw)
        if report["measured_fields"]<600:raise ValueError("Short Mednafen interval")
        report.update(elf_sha256=sha(elf),bios_sha256=sha(bios),mednafen_sha256=sha(Path(exe)),
                      wram_order=order,command=command,state_sha256=sha(states[0]))
        (case/"mednafen-analysis.json").write_text(json.dumps(report,indent=2)+"\n")
        return report


def compare(a,b):
    check_ntsc_calibration(a,b)
    for key in ("bios_sha256","mednafen_sha256"):
        if not a.get(key) or a[key]!=b.get(key):raise ValueError("Mismatched Mednafen provenance "+key)
    for key in ("stage","divisions","hud","vertices_per_actor","faces_per_actor"):
        if a["header"][key]!=b["header"][key]:
            raise ValueError("Mismatched workload "+key)
    for key in ("faces","commands"):
        if a[key]!=b[key]:raise ValueError("Mismatched "+key)
    costs={}
    for key in ("geometry","flush","work"):
        x,y=(r["stage_us"][key]["median"] for r in (a,b))
        costs[key]=dict(control_us=x,candidate_us=y,reduction_percent=100*(x-y)/x if x else None)
    return dict(costs=costs,control_submit_s=a["submissions_per_nominal_second"],
                candidate_submit_s=b["submissions_per_nominal_second"],
                control_frt_hz=a["header"]["frequency"],candidate_frt_hz=b["header"]["frequency"])


def check_ntsc_calibration(a,b):
    # sat_time_frequency_hz() = integer ticks_per_frame * refresh_hz.
    # A difference of one FRT tick per NTSC field is expected from
    # independent phase/quantization at VBlank calibration. Reject larger
    # differences rather than silently accepting a different clock policy.
    x,y=(r["header"]["frequency"] for r in (a,b))
    if min(x,y)<=0 or x%60 or y%60 or abs(x-y)>60:
        raise ValueError(f"Incompatible NTSC FRT calibration: {x} vs {y} Hz")


def check_retained(run,bios,probe):
    matrices={v:load_reports([run/v/"matrix.json"]) for v in ("control","candidate")}
    for v,cases in matrices.items():
        if set(cases)!=set(CASES):raise ValueError("Incomplete retained cases: "+v)
        for name,(r,case) in cases.items():
            if r.get("static_project_experiment")!=(v=="candidate") or r.get("rotated_camera") is not True:
                raise ValueError("Unexpected retained variant/camera: "+str(case))
            if r["provenance"]["bios"]!=sha(bios) or r["provenance"]["probe"]!=sha(probe):
                raise ValueError("Retained BIOS or probe differs")
            # Bind retained timing to the linked ELF and original capture.
            syms=symbols(case/"cube_pipeline_bench.elf","sh2eb-elf-nm")
            if ("sat_static_project_sh2" in syms)!=(v=="candidate"):
                raise ValueError("Retained variant symbol differs")
            validate_panic((case/"native-panic.bin").read_bytes(),syms["g_sat_last_panic"][1])
            report=decode((case/"telemetry.bin").read_bytes())
            for key in ("header","stage_us","commands","faces","measured_fields","submissions_per_nominal_second"):
                if report[key]!=r[key]:raise ValueError("Retained telemetry differs: "+key)
            # Verify the disc app matches the ELF-derived retained app
            # before booting it natively. The save-state reader also
            # verifies the loaded .text against this ELF.
            for suffix in (".cue",".bin",".iso",".app.bin"):
                if not (case/("cube_pipeline_bench"+suffix)).is_file():
                    raise ValueError("Missing retained disc artifact: "+suffix)
    for name in CASES:
        a,b=(matrices[v][name][0] for v in ("control","candidate"))
        check_ntsc_calibration(a,b)
        for key in ("source","mesh","makefile","runner"):
            if not a["provenance"].get(key) or a["provenance"][key]!=b["provenance"].get(key):
                raise ValueError("Retained build input differs: "+key)
        if a.get("revision")!=b.get("revision"):
            raise ValueError("Retained revisions differ")
    return {name:matrices["control"][name][0]["revision"] for name in CASES}


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--resume",type=Path,help="reuse a completed Ymir run directory and execute native Mednafen")
    args=parser.parse_args()
    OUT.mkdir(parents=True,exist_ok=True)
    run=args.resume.resolve() if args.resume else Path(tempfile.mkdtemp(prefix="run-",dir=OUT))
    if args.resume and (run.parent!=OUT.resolve() or not run.is_dir()):
        parser.error("--resume must identify an existing child of "+str(OUT))
    archive=OUT.parent/"static_project_renderer-return.zip"
    # Keep the previous ZIP until the replacement is ready.
    env=dict(os.environ)
    tools=Path(env.get("LIBSATURN_TOOLCHAIN_ROOT",str(Path.home()/"dev/saturn-tools")))/"bin"
    env["PATH"]=str(tools)+os.pathsep+env.get("PATH","")
    os.environ["PATH"]=env["PATH"]
    bios=Path(env.get("LIBSATURN_BIOS",str(ROOT/"bios/firmware/mpr-17933.bin"))).resolve()
    probe=Path(env.get("LIBSATURN_YMIR_BUILD_DIR",str(ROOT/"harness/build-macos")))/"probe"
    summary={"checkout":subprocess.check_output(["git","rev-parse","HEAD"],cwd=ROOT,text=True).strip(),
             "cases":{},"completed":False}
    (run/"failure.log").unlink(missing_ok=True)
    try:
        summary["resumed"]=bool(args.resume)
        for variant in (() if args.resume else ("control","candidate")):
            cmd=[sys.executable,str(ROOT/"harness/scripts/cube_pipeline.py"),"--out",str(run/variant),
                 "--bios",str(bios),"--probe",str(probe),"--compiler-dir",str(tools),
                 "--frames","1200","--rotated-camera","--cases",*CASES]
            if variant=="candidate":cmd.append("--static-project-experiment")
            print("Ymir:",variant,flush=True)
            with (run/(variant+".log")).open("w") as log:
                subprocess.run(cmd,cwd=ROOT,env=env,stdout=log,stderr=subprocess.STDOUT,check=True)
        summary["ymir_build_revisions"]=check_retained(run,bios,probe)
        # Explicit, bounded normalization after checking both builds share
        # the NTSC field-calibration policy. Call directly so any exception
        # is retained, not hidden behind CalledProcessError.
        ymir=compare_ymir([run/"control/matrix.json"],run/"candidate/matrix.json",
                          normalize_calibration=True)
        (run/"ymir-compare.json").write_text(json.dumps(ymir,indent=2)+"\n")
        if not ymir["passed"]:raise ValueError("Ymir model pixels differ")
        summary["ymir"]=ymir
        for name in CASES:
            reports=[]
            for variant in ("control","candidate"):
                print("Mednafen:",variant,name,flush=True)
                reports.append(capture_med(run/variant/name,bios,env,run))
            summary["cases"][name]={"mednafen":compare(*reports)}
        summary["completed"]=True
    except BaseException as exc:
        import traceback
        summary["error"]=str(exc)
        (run/"failure.log").write_text(traceback.format_exc())
        raise
    finally:
        (run/"summary.json").write_text(json.dumps(summary,indent=2)+"\n")
        temporary_archive=archive.with_suffix(".tmp.zip")
        with zipfile.ZipFile(temporary_archive,"w",zipfile.ZIP_DEFLATED) as z:
            for p in run.rglob("*"):
                if p.is_file() and (p.suffix in (".json",".png",".log") or p.name in
                                    ("telemetry.bin","mednafen-telemetry.bin","native-panic.bin",
                                     "mednafen-panic.bin","working-ram.bin","working-mask.bin")):
                    z.write(p,p.relative_to(run))
        temporary_archive.replace(archive)
        print("Return:",archive,flush=True)

if __name__=="__main__":
    main()
