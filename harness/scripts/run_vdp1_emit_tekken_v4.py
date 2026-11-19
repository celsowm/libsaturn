#!/usr/bin/env python3
"""Matched C3 SDK A/B in an existing private Tekken 3 Release workload."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import traceback
import zipfile

from renderer_visual import load_rgb,diff,write_rgb

ROOT=Path(__file__).resolve().parents[2]

def sha(path):
    with path.open("rb") as stream:
        digest=hashlib.sha256()
        for chunk in iter(lambda:stream.read(1024*1024),b""):
            digest.update(chunk)
        return digest.hexdigest()

def clean_revision(repo):
    status=subprocess.check_output(["git","status","--porcelain"],cwd=repo,text=True)
    if status.strip():raise ValueError("Use a clean committed checkout: "+str(repo))
    return subprocess.check_output(["git","rev-parse","HEAD"],cwd=repo,text=True).strip()

def dump(path,value):
    path.write_text(json.dumps(value,indent=2)+"\n")

def execute(cmd,log,env,timeout):
    print("Running:",*map(str,cmd[:4]),flush=True)
    with log.open("w") as stream:
        stream.write(repr(cmd)+"\n");stream.flush()
        subprocess.run(cmd,cwd=ROOT,env=env,stdout=stream,stderr=subprocess.STDOUT,
                       check=True,timeout=timeout)

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--tekken",type=Path,required=True)
    p.add_argument("--baseline",type=Path,required=True,help="Existing optimized Tekken build containing CMakeCache.txt")
    p.add_argument("--bios",type=Path,required=True)
    p.add_argument("--device-script",type=Path,required=True,help="Existing combat input tape in Tekken probe device format")
    p.add_argument("--frames",type=int,nargs="+",default=[3000,8000,30000])
    p.add_argument("--workload",choices=["diagnostic","two-paul-release","paul-law-release"],default="diagnostic")
    p.add_argument("--timeout",type=int,default=1800)
    p.add_argument("--jobs",type=int,default=4)
    p.add_argument("--game-build-root",type=Path,help="Game build parent; Tekken asset tools only write under <tekken>/build")
    a=p.parse_args()
    if sorted(set(a.frames))!=a.frames or min(a.frames)<300 or a.jobs<1 or a.timeout<1:
        p.error("Use increasing distinct field counts >=300 and positive jobs/timeout")
    tekken=a.tekken.resolve();baseline=a.baseline.resolve();bios=a.bios.resolve()
    tape=a.device_script.resolve()
    lib_rev=clean_revision(ROOT);game_rev=clean_revision(tekken)
    sys.path.insert(0,str(tekken/"scripts"))
    from run_saturn_stage_matrix import cache_options
    options,compiler=cache_options(baseline/"CMakeCache.txt")
    # Replicate the original game cache, except SDK location. The asset
    # tooling checkout is explicitly this same clean LibSaturn revision.
    options=[o for o in options if o.split(":",1)[0] not in
             ("-DLibSaturn_DIR","-DCMAKE_PREFIX_PATH","-DLIBSATURN_SOURCE_DIR")]
    env=dict(os.environ)
    env["PATH"]=str(compiler)+os.pathsep+env.get("PATH","")
    env["LIBSATURN_TOOLCHAIN_ROOT"]=str(compiler)
    out=ROOT/"harness/build";out.mkdir(parents=True,exist_ok=True)
    run=Path(tempfile.mkdtemp(prefix="vdp1-tekken-v4-",dir=out))
    summary=dict(completed=False,libsaturn=lib_rev,tekken=game_rev,
        bios_sha256=sha(bios),input_sha256=sha(tape),frames=a.frames,
        workload=a.workload,modes={},comparisons={},
        scope="Matched SDKs, scripted real game. End-field image mismatch requires matched-state review.")
    try:
        for mode in ("control","hybrid"):
            case=run/mode;case.mkdir()
            sdk_build=case/"sdk-build";prefix=case/"sdk"
            game=(a.game_build_root.resolve()/f"{run.name}-{mode}") if a.game_build_root else case/"game"
            commands=[
                ["cmake","-S",str(ROOT),"-B",str(sdk_build),"-G","Ninja",
                 "-DCMAKE_BUILD_TYPE=Release","-DCMAKE_TOOLCHAIN_FILE="+str(ROOT/"cmake/toolchains/sh2eb-elf.cmake"),
                 "-DLIBSATURN_TOOLCHAIN_ROOT="+str(compiler),
                 "-DLIBSATURN_VDP1_HYBRID_EMIT="+("ON" if mode=="hybrid" else "OFF"),
                 "-DLIBSATURN_ENABLE_PROFILE_METRICS=OFF","-DLIBSATURN_INSTALL=ON",
                 "-DLIBSATURN_BUILD_TESTS=OFF"],
                ["cmake","--build",str(sdk_build),"--parallel",str(a.jobs)],
                ["cmake","--install",str(sdk_build),"--prefix",str(prefix)]]
            for i,cmd in enumerate(commands):execute(cmd,case/f"sdk-{i}.log",env,a.timeout)
            configs=list(prefix.rglob("LibSaturnConfig.cmake"))
            archives=list(prefix.rglob("libsaturn.a"))
            if len(configs)!=1 or len(archives)!=1:raise ValueError("Ambiguous installed SDK")
            sdk_hash=sha(archives[0])
            cmd=["cmake","-S",str(tekken),"-B",str(game),"-G","Ninja",*options,
                 "-DLibSaturn_DIR="+str(configs[0].parent),"-DLIBSATURN_SOURCE_DIR="+str(ROOT)]
            execute(cmd,case/"game-config.log",env,a.timeout)
            execute(["cmake","--build",str(game),"--parallel",str(a.jobs)],case/"game-build.log",env,a.timeout)
            disc=game/"disc/music.cue"
            if not disc.is_file():disc=game/"disc/tekken3_saturn.iso"
            captures={}
            summary["modes"][mode]=dict(sdk_sha256=sdk_hash,captures=captures)
            for count in a.frames:
                capture=case/f"capture-{count}"
                cmd=[sys.executable,str(tekken/"scripts/saturn_probe.py"),"--build",str(game),
                     "--disc",str(disc),"--libsaturn",str(ROOT),"--bios",str(bios),
                     "--device-script",str(tape),"--workload",a.workload,"--frames",str(count),
                     "--sh2-cache","on","--timeout",str(a.timeout),"--out",str(capture)]
                execute(cmd,case/f"capture-{count}.log",env,a.timeout+60)
                data=json.loads((capture/"observation.json").read_text())
                if data["workload"].get("sdk_archive_sha256")!=sdk_hash:
                    raise ValueError("Game did not report expected SDK hash")
                state=data["state"]
                if state["sim_ticks"]<1 or state["faces_rejected"] or state["draw_timeouts"] or state["first_error"]:
                    raise ValueError("Gameplay/render gate failed")
                captures[str(count)]={"state":state,"workload":data["workload"],
                    "screen_sha256":sha(capture/"screen.png")}
                dump(run/"summary.json",summary)
            # Keep textual map for auditing RAM, not the private binary/disc.
            maps=list(game.glob("tekken3_saturn.map"))
            if maps:shutil.copyfile(maps[0],case/"game.map.txt")
        control,hybrid=(summary["modes"][v] for v in ("control","hybrid"))
        if control["sdk_sha256"]==hybrid["sdk_sha256"]:raise ValueError("SDK archives identical")
        dest=run/"diff";dest.mkdir()
        for count in a.frames:
            x,y=(v["captures"][str(count)] for v in (control,hybrid))
            w1,w2=(v["workload"] for v in (x,y))
            if {k:v for k,v in w1.items() if k!="sdk_archive_sha256"}!={k:v for k,v in w2.items() if k!="sdk_archive_sha256"}:
                raise ValueError("Game workload differs between SDKs")
            s,t=x["state"],y["state"]
            keys=("sim_ticks","frame_serial","faces_submitted","faces_flushed","commands_used",
                  "late_swaps","draw_waits","draw_timeouts","health","hits","blocks","round","timer")
            comparison={k:{"control":s[k],"hybrid":t[k]} for k in keys}
            comparison["same_sampled_combat_state"]=all(s[k]==t[k] for k in
                ("sim_ticks","health","hits","blocks","round","timer","fighter_x","fighter_z"))
            comparison["pixels"]={"status":"REVIEW_RASTER"}
            paths=[run/v/f"capture-{count}/screen.png" for v in ("control","hybrid")]
            w,h,raw=load_rgb(paths[0]);w2,h2,other=load_rgb(paths[1])
            if (w,h)==(w2,h2):
                pixels,mask=diff(raw,other,w,h)
                write_rgb(dest/f"field-{count}.png",w,h,mask)
                comparison["pixels"]=pixels
            summary["comparisons"][str(count)]=comparison
        summary["completed"]=True
        summary["combat_activity_observed"]=any(c["state"]["hits"] or c["state"]["blocks"]
            for c in control["captures"].values())
        summary["promotion_allowed"]=False
    except BaseException as exc:
        summary["error"]=str(exc)
        (run/"failure.log").write_text(traceback.format_exc())
        raise
    finally:
        dump(run/"summary.json",summary)
        archive=out/"vdp1_emit_tekken_v4-return.zip"
        with zipfile.ZipFile(archive,"w",zipfile.ZIP_DEFLATED) as z:
            for path in run.rglob("*"):
                if not path.is_file():continue
                rel=path.relative_to(run)
                # No private ROM assets, SDK binaries, game binaries or input tape.
                if path==run/"summary.json" or path==run/"failure.log" or (
                    len(rel.parts)==2 and path.suffix in (".log",".txt")) or (
                    len(rel.parts)==3 and rel.parts[1].startswith("capture-") and
                    path.name in ("observation.json","screen.png")) or (
                    rel.parts[0]=="diff" and path.suffix==".png"):
                    z.write(path,rel)
        print("Return:",archive,flush=True)

if __name__=="__main__":main()
