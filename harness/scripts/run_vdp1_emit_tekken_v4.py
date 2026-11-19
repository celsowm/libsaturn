#!/usr/bin/env python3
"""Matched C3 SDK A/B in an existing private Tekken 3 Release workload.

Per mode (control, hybrid) from one clean revision: build and install the SDK,
build the game from the baseline cache, then
- live captures at each --frames count, --repeats times (cadence and state);
- optionally one bounded --debug-profile capture of the same live build
  (intrusive; never mixed with cadence numbers);
- optionally, for each --freeze-ticks T, rebuild the same game directory with
  T3_RENDER_BENCH_TICK=T and capture the frozen scene with VDP1 VRAM and the
  raw framebuffer. Both modes then hold the same logical tick, so command
  lists, Gouraud tables and pixels are compared at a matched state.
With a simulation-tick input fixture (--game-define T3_LOCAL_INPUT_TAPE=...),
both builds replay identical inputs on identical ticks.
"""
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
# Counters that follow host/field timing rather than the simulated state.
TIMING_KEYS={"frame_serial","vblanks","sim_steps_last_frame","loop_rate_x10","frame_work_ms",
    "late_swaps","draw_waits","snapshot_epoch","skin_work_ms","render_work_ms","submitted_frames",
    "elapsed_fields_last","observed_fields","discarded_fields","images_at_tick_mark",
    "fields_at_tick_mark","local_pose_cache","pad_held","pad_pressed"}
EMITTERS=("push_projected_program","push_projected_sprite","push_distorted_sprite")

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

def logical(state):
    return {k:v for k,v in state.items() if k not in TIMING_KEYS}

def emitter_costs(profile):
    """Exclusive and completed-call costs of the hybrid-affected emitters per CPU."""
    result={}
    for cpu,data in profile["cpus"].items():
        total=data["totals"]["execution_cycles"]
        rows={}
        for item in data["functions"]:
            hit=next((e for e in EMITTERS if "vdp1::"+e+"(" in item["symbol"]),None)
            if hit:rows.setdefault(hit,dict(exclusive_cycles=0))["exclusive_cycles"]+=item["execution_cycles"]
        for item in profile["completed_calls"].get(cpu,{}).get("functions",[]):
            hit=next((e for e in EMITTERS if "vdp1::"+e+"(" in item["symbol"]),None)
            if hit:
                row=rows.setdefault(hit,dict(exclusive_cycles=0))
                row["calls"]=row.get("calls",0)+item["calls"]
                row["inclusive_cycles"]=row.get("inclusive_cycles",0)+item["inclusive_cpu_cost"]
        share=sum(r["exclusive_cycles"] for r in rows.values())
        result[cpu]=dict(total_cycles=total,emitters=rows,exclusive_cycles=share,
                         percent=100*share/total if total else 0,
                         top=[dict(symbol=i["symbol"],cycles=i["execution_cycles"],
                                   percent=round(i["percent_execution_cycles"],2))
                              for i in data["functions"][:25]])
    return result

def main():
    p=argparse.ArgumentParser(description=__doc__,formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--tekken",type=Path,required=True)
    p.add_argument("--baseline",type=Path,required=True,help="Existing optimized Tekken build containing CMakeCache.txt")
    p.add_argument("--bios",type=Path,required=True)
    p.add_argument("--device-script",type=Path,required=True,help="Existing combat input tape in Tekken probe device format")
    p.add_argument("--frames",type=int,nargs="*",default=[3000,8000,30000])
    p.add_argument("--repeats",type=int,default=1,help="Live captures per field count and mode")
    p.add_argument("--freeze-ticks",type=int,nargs="*",default=[],help="Matched-state captures (T3_RENDER_BENCH_TICK, 1..3600)")
    p.add_argument("--freeze-margin",type=int,default=1500,help="Fields beyond the frozen tick before the capture")
    p.add_argument("--profile-frames",type=int,nargs=3,metavar=("FIELDS","FROM","COUNT"),
                   help="One --debug-profile live capture: run FIELDS, trace COUNT program frames from FROM")
    p.add_argument("--trace-capacity",type=int,default=1048576)
    p.add_argument("--workload",choices=["diagnostic","two-paul-release","paul-law-release"],default="diagnostic")
    p.add_argument("--timeout",type=int,default=1800)
    p.add_argument("--jobs",type=int,default=4)
    p.add_argument("--game-build-root",type=Path,help="Game build parent; Tekken asset tools only write under <tekken>/build")
    p.add_argument("--game-define",action="append",default=[],metavar="KEY=VALUE",
                   help="Extra Tekken CMake cache entry, applied identically to both builds (e.g. T3_BOOT_FIGHTERS=0;1)")
    a=p.parse_args()
    if sorted(set(a.frames))!=a.frames or (a.frames and min(a.frames)<300) or a.jobs<1 or a.timeout<1 or a.repeats<1:
        p.error("Use increasing distinct field counts >=300, positive jobs/timeout/repeats")
    if any(not 0<t<=3600 for t in a.freeze_ticks) or sorted(set(a.freeze_ticks))!=a.freeze_ticks:
        p.error("Freeze ticks must be increasing distinct values in 1..3600")
    if not (a.frames or a.freeze_ticks or a.profile_frames):p.error("Nothing to capture")
    tekken=a.tekken.resolve();baseline=a.baseline.resolve();bios=a.bios.resolve()
    tape=a.device_script.resolve()
    lib_rev=clean_revision(ROOT);game_rev=clean_revision(tekken)
    sys.path.insert(0,str(tekken/"scripts"))
    from run_saturn_stage_matrix import cache_options
    import compare_vdp1_vram
    options,compiler=cache_options(baseline/"CMakeCache.txt")
    # Replicate the original game cache, except SDK location. The asset
    # tooling checkout is explicitly this same clean LibSaturn revision.
    options=[o for o in options if o.split(":",1)[0] not in
             ("-DLibSaturn_DIR","-DCMAKE_PREFIX_PATH","-DLIBSATURN_SOURCE_DIR")]
    overrides=dict(d.split("=",1) for d in a.game_define)
    overrides.setdefault("T3_RENDER_BENCH_TICK","0")
    options=[o for o in options if o.split(":",1)[0][2:] not in overrides]
    options+=["-D%s=%s"%kv for kv in sorted(overrides.items()) if kv[0]!="T3_RENDER_BENCH_TICK"]
    fixture=overrides.get("T3_LOCAL_INPUT_TAPE")
    env=dict(os.environ)
    env["PATH"]=str(compiler)+os.pathsep+env.get("PATH","")
    env["LIBSATURN_TOOLCHAIN_ROOT"]=str(compiler)
    out=ROOT/"harness/build";out.mkdir(parents=True,exist_ok=True)
    run=Path(tempfile.mkdtemp(prefix="vdp1-tekken-v4-",dir=out))
    summary=dict(completed=False,libsaturn=lib_rev,tekken=game_rev,
        bios_sha256=sha(bios),input_sha256=sha(tape),
        tick_fixture_sha256=sha(Path(fixture)) if fixture else None,
        frames=a.frames,repeats=a.repeats,freeze_ticks=a.freeze_ticks,profile_frames=a.profile_frames,
        game_defines=overrides,workload=a.workload,modes={},comparisons={},frozen={},
        scope="Matched SDKs, scripted real game. Live end-field images compare equal fields, "
              "not equal states; frozen captures compare equal simulation ticks.")

    def capture(mode,game,disc,sdk_hash,name,fields,workload,extra=()):
        dest=run/mode/name
        cmd=[sys.executable,str(tekken/"scripts/saturn_probe.py"),"--build",str(game),
             "--disc",str(disc),"--libsaturn",str(ROOT),"--bios",str(bios),
             "--device-script",str(tape),"--workload",workload,"--frames",str(fields),
             "--sh2-cache","on","--timeout",str(a.timeout),"--out",str(dest),*extra]
        execute(cmd,run/mode/f"{name}.log",env,a.timeout+60)
        data=json.loads((dest/"observation.json").read_text())
        if data["workload"].get("sdk_archive_sha256")!=sdk_hash:
            raise ValueError("Game did not report expected SDK hash")
        state=data["state"]
        if state["sim_ticks"]<1 or state["faces_rejected"] or state["draw_timeouts"] or state["first_error"]:
            raise ValueError("Gameplay/render gate failed: "+name)
        return dest,dict(state=state,workload=data["workload"],screen_sha256=sha(dest/"screen.png"))

    def configure(game,tick):
        cmd=["cmake","-S",str(tekken),"-B",str(game),"-G","Ninja",*options,
             "-DT3_RENDER_BENCH_TICK="+str(tick),
             "-DLibSaturn_DIR="+str(configs[0].parent),"-DLIBSATURN_SOURCE_DIR="+str(ROOT)]
        execute(cmd,run/mode/f"game-config-{tick}.log",env,a.timeout)
        execute(["cmake","--build",str(game),"--parallel",str(a.jobs)],run/mode/f"game-build-{tick}.log",env,a.timeout)
        disc=game/"disc/music.cue"
        return disc if disc.is_file() else game/"disc/tekken3_saturn.iso"

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
            captures={};frozen={}
            summary["modes"][mode]=dict(sdk_sha256=sdk_hash,captures=captures,frozen=frozen)
            if a.frames or a.profile_frames:
                disc=configure(game,0)
                summary["modes"][mode]["elf_sha256"]=sha(game/"tekken3_saturn.elf")
                # Keep textual map for auditing RAM, not the private binary/disc.
                maps=list(game.glob("tekken3_saturn.map"))
                if maps:shutil.copyfile(maps[0],case/"game.map.txt")
                for count in a.frames:
                    for r in range(a.repeats):
                        _,result=capture(mode,game,disc,sdk_hash,f"capture-{count}-r{r}",count,a.workload)
                        captures.setdefault(str(count),[]).append(result)
                        dump(run/"summary.json",summary)
                if a.profile_frames:
                    fields,start,count=a.profile_frames
                    dest,result=capture(mode,game,disc,sdk_hash,"profile",fields,"diagnostic",
                        ["--debug-profile","--trace-from",str(start),"--trace-frames",str(count),
                         "--trace-capacity",str(a.trace_capacity)])
                    result["emitters"]=emitter_costs(json.loads((dest/"profile-summary.json").read_text()))
                    summary["modes"][mode]["profile"]=result
                    dump(run/"summary.json",summary)
            for tick in a.freeze_ticks:
                disc=configure(game,tick)
                dest,result=capture(mode,game,disc,sdk_hash,f"frozen-{tick}",tick+a.freeze_margin,
                                    "diagnostic",["--dump-vdp1"])
                if result["state"]["sim_ticks"]!=tick:
                    raise ValueError(f"frozen capture stopped at tick {result['state']['sim_ticks']}, not {tick}")
                result["vdp1_vram_sha256"]=sha(dest/"vdp1-vram.bin")
                result["vdp1_fb_sha256"]=sha(dest/"vdp1-fb.bin")
                frozen[str(tick)]=result
                dump(run/"summary.json",summary)
        control,hybrid=(summary["modes"][v] for v in ("control","hybrid"))
        if control["sdk_sha256"]==hybrid["sdk_sha256"]:raise ValueError("SDK archives identical")
        dest=run/"diff";dest.mkdir()
        def pixels(name,paths):
            w,h,raw=load_rgb(paths[0]);w2,h2,other=load_rgb(paths[1])
            if (w,h)!=(w2,h2):return {"status":"REVIEW_RASTER"}
            result,mask=diff(raw,other,w,h)
            write_rgb(dest/f"{name}.png",w,h,mask)
            return result
        for mode in ("control","hybrid"):
            for count,runs in summary["modes"][mode]["captures"].items():
                summary["modes"][mode].setdefault("repeat_identical",{})[count]=all(
                    r["state"]==runs[0]["state"] and r["screen_sha256"]==runs[0]["screen_sha256"] for r in runs)
        for count in a.frames:
            x,y=(v["captures"][str(count)][0] for v in (control,hybrid))
            w1,w2=(v["workload"] for v in (x,y))
            if {k:v for k,v in w1.items() if k!="sdk_archive_sha256"}!={k:v for k,v in w2.items() if k!="sdk_archive_sha256"}:
                raise ValueError("Game workload differs between SDKs")
            s,t=x["state"],y["state"]
            keys=("sim_ticks","frame_serial","faces_submitted","faces_flushed","commands_used",
                  "late_swaps","draw_waits","draw_timeouts","health","hits","blocks","round","timer",
                  "images_at_tick_mark","fields_at_tick_mark")
            comparison={k:{"control":s[k],"hybrid":t[k]} for k in keys}
            comparison["same_logical_state"]=logical(s)==logical(t)
            comparison["pixels"]=pixels(f"field-{count}",[run/v/f"capture-{count}-r0/screen.png" for v in ("control","hybrid")])
            summary["comparisons"][str(count)]=comparison
        for tick in a.freeze_ticks:
            s,t=(v["frozen"][str(tick)]["state"] for v in (control,hybrid))
            names=[run/v/f"frozen-{tick}" for v in ("control","hybrid")]
            ls,lt=logical(s),logical(t)
            result=dict(same_logical_state=ls==lt,differing_keys=sorted(k for k in ls if ls[k]!=lt.get(k)),
                commands_used={"control":s["commands_used"],"hybrid":t["commands_used"]})
            try:
                compare_vdp1_vram.compare(names[0]/"vdp1-vram.bin",names[1]/"vdp1-vram.bin",True)
                result["command_list"]="MATCH"
            except ValueError as exc:result["command_list"]="DIFF: "+str(exc)
            dump(dest/f"frozen-{tick}-commands.json",{v:compare_vdp1_vram.normalized(n/"vdp1-vram.bin")
                                                      for v,n in zip(("control","hybrid"),names)})
            fb=[(n/"vdp1-fb.bin").read_bytes() for n in names]
            result["framebuffer"]=dict(bytes=len(fb[0]),differing_bytes=sum(x!=y for x,y in zip(*fb))+abs(len(fb[0])-len(fb[1])))
            result["vram_identical"]=control["frozen"][str(tick)]["vdp1_vram_sha256"]==hybrid["frozen"][str(tick)]["vdp1_vram_sha256"]
            result["pixels"]=pixels(f"frozen-{tick}",[n/"screen.png" for n in names])
            summary["frozen"][str(tick)]=result
        if a.profile_frames:
            summary["profile"]={v:summary["modes"][v]["profile"]["emitters"] for v in ("control","hybrid")}
        summary["completed"]=True
        summary["combat_activity_observed"]=any(c["state"]["hits"] or c["state"]["blocks"]
            for runs in control["captures"].values() for c in runs)
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
                # No private ROM assets, SDK binaries, game binaries, dumps or input tape.
                if path==run/"summary.json" or path==run/"failure.log" or (
                    len(rel.parts)==2 and path.suffix in (".log",".txt")) or (
                    len(rel.parts)==3 and rel.parts[1].startswith(("capture-","frozen-","profile")) and
                    path.name in ("observation.json","screen.png","profile-summary.json")) or (
                    rel.parts[0]=="diff" and path.suffix in (".png",".json")):
                    z.write(path,rel)
        print("Return:",archive,flush=True)

if __name__=="__main__":main()
