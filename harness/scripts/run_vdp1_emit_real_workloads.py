#!/usr/bin/env python3
"""C3 V3: matched real-example control/hybrid runs, not a promotion gate."""
import argparse
import json
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import tempfile
import traceback
import zipfile

from cube_pipeline import ROOT, build_lock, symbols, validate_panic
from local_changes import capture, add_to_zip
from renderer_pipeline_study import study
from renderer_visual import require_ffmpeg, load_rgb, diff, write_rgb, sha
import run_hierarchy_character as shared

EXAMPLES=("dino_demo","skeletal_animation_3d","skinning_stress_3d",
          "fighter_arena_3d","world_lod3d_demo")
MODES={"control":1,"hybrid":3}
COUNTERS=("g_list_serial","g_draw_waits","g_draw_timeouts","g_late_swaps")
EMITTERS=("push_distorted_sprite","push_projected_sprite","push_projected_program")
CHECKPOINTS=(600,1200)
PROFILE_FIELDS=12

def dump(path,obj):
    path.write_text(json.dumps(obj,indent=2)+"\n")

def data_symbols(elf):
    syms=symbols(elf,"sh2eb-elf-nm")
    result={"g_sat_last_panic":syms["g_sat_last_panic"]}
    for name in COUNTERS:
        # Anonymous namespace variables are mangled; resolve their actual
        # ELF names rather than relying on a hard-coded WRAM address.
        hits=[(k,v) for k,v in syms.items() if k==name or
              re.search(r"\d"+re.escape(name)+r"E(?:\..*)?$",k)]
        if len(hits)!=1 or hits[0][1][1]!=4:
            raise ValueError("Missing or ambiguous counter: "+name)
        result[name]=hits[0][1]
    return result,syms

def read_counters(folder,syms):
    validate_panic((folder/"panic.bin").read_bytes(),syms["g_sat_last_panic"][1])
    counts={name:struct.unpack(">I",(folder/(name+".bin")).read_bytes())[0]
            for name in COUNTERS}
    if counts["g_draw_timeouts"]:
        raise ValueError("VDP1 draw timeout")
    if counts["g_list_serial"]==0:
        raise ValueError("No submitted VDP1 lists")
    return counts

def build(case,example,mode,env,fixed_pose=False):
    case.mkdir(parents=True)
    flags={key:"0" for key in ("SAT_NEAR_CLIP_EXPERIMENT","SAT_WORLD_CULL_EXPERIMENT",
           "SAT_SCREEN_CLIP_EXPERIMENT","SAT_STATIC_PROJECT_EXPERIMENT",
           "SAT_HIERARCHY_EXPERIMENT","SAT_SKIN_BLEND_BENCH",
           "SAT_SKIN_CHARACTER_EXPERIMENT","HIERARCHY_CHARACTER_BENCH",
           "SKIN_CHARACTER_BENCH","SKELETAL_ROBOT_DEMO")}
    flags["SAT_VDP1_EMIT_EXPERIMENT"]=str(mode)
    flags["SAT_VDP1_POSE_CAPTURE"]="1" if fixed_pose else "0"
    if fixed_pose:
        flags.update(HIERARCHY_CHARACTER_BENCH="1",SAT_HIERARCHY_EXPERIMENT="1")
    if example=="skeletal_animation_3d":
        flags["SKELETAL_ROBOT_DEMO"]="1"
    if example=="dino_demo":
        # 1822 upload descriptors; move only that table, retain hot programs.
        flags.update(DINO_HIGH_TEXTURE_DESCRIPTORS="0",DINO_HIGH_MATERIAL_PROGRAMS="1",DINO_MUSIC_CDDA="1")
    if example in ("skinning_stress_3d","fighter_arena_3d"):
        flags["CHARACTER_DEMO_DISC_ASSETS"]="0"
        flags["CHARACTER_DEMO_STRESS_INITIAL_PRESET"]="2" if example=="skinning_stress_3d" else "0"
    local=dict(env,**flags)
    with build_lock(example):
        shared.command_run(["sh","build-example.sh",example],case/"build.log",local)
        for ext in (".elf",".map",".app.bin",".iso",".bin",".cue"):
            shutil.copyfile(ROOT/"build"/(example+ext),case/(example+ext))
    elf=case/(example+".elf")
    dump(case/"build.json",dict(example=example,mode=mode,flags=flags,
         hashes={ext:sha(case/(example+ext)) for ext in (".elf",".app.bin",".bin",".cue")},
         compiler=subprocess.check_output(["sh2eb-elf-gcc","--version"],env=local,text=True).splitlines()[0]))
    (case/"disassembly.log").write_text(subprocess.check_output(
        ["sh2eb-elf-objdump","-d","-C",str(elf)],env=local,text=True))
    return elf

def profile_summary(report):
    fields=report["frame_observations"]
    n=fields["frame_snapshots"]
    if n!=PROFILE_FIELDS:
        raise ValueError("Unexpected profile window size")
    cpus={}
    for cpu in ("master","slave"):
        data=report["cpus"][cpu]
        emit=[f for f in data["functions"] if any(
            "saturn::hal::vdp1::"+name+"(" in f["symbol"] for name in EMITTERS)]
        calls=sum(e["calls"] for e in report["call_edges"] if e["cpu"]==cpu and any(
            "saturn::hal::vdp1::"+name+"(" in e["callee"] for name in EMITTERS))
        cycles=sum(f["execution_cycles"] for f in emit)
        cpus[cpu]=dict(execution_cycles=data["totals"]["execution_cycles"],
             instructions=data["totals"]["instructions"],target_emitter_cycles=cycles,
             observed_call_edges=calls,
             cycles_per_observed_call_edge=cycles/calls if calls else None,
             emit_functions=emit)
    return dict(cpus=cpus,frame_observations=fields,
         scope="Exclusive interpreter PC cost and observed call edges; not inclusive emitter time.")

def run_ymir(case,example,elf,probe,bios,env):
    counter_syms,syms=data_symbols(elf)
    rows=[]
    for end in CHECKPOINTS:
        folder=case/"ymir"/str(end);folder.mkdir(parents=True)
        disc=case/(example+(".cue" if example=="dino_demo" else ".iso"))
        cmd=[str(probe),"--iso",str(disc),"--bin",str(case/(example+".app.bin")),
             "--bios",str(bios),"--slave-reset-entry",hex(syms["saturn_slave_entry"][0]),
             "--boot-frames","90","--frames",str(end),"--sh2-cache","on",
             "--out",str(folder/"probe.json"),"--screenshot",f"{end-1}:{folder/'screen.png'}"]
        for name,(address,size) in counter_syms.items():
            target="panic.bin" if name=="g_sat_last_panic" else name+".bin"
            cmd+=["--dump-mem",f"{address:#x}:{size}:{folder/target}"]
        if example=="world_lod3d_demo":
            cmd+=["--pad-script",str(ROOT/"harness/scripts/world_lod3d_demo_route.pad")]
        if example=="skinning_stress_3d":
            hits=[v for k,v in syms.items() if k=="g_actor_count" or re.search(r"\d+g_actor_countE(?:\..*)?$",k)]
            if len(hits)!=1 or hits[0][1]!=4:raise ValueError("Missing actor count")
            address,size=hits[0]
            cmd+=["--dump-mem",f"{address:#x}:{size}:{folder/'actors.bin'}"]
        if end==CHECKPOINTS[-1]:
            cmd+=["--debug-profile",str(folder/"debug"),"--trace-from",str(end-40),
                  "--trace-frames",str(PROFILE_FIELDS),"--trace-capacity","1048576"]
        shared.command_run(cmd,folder/"run.log",env,timeout=900)
        row=dict(fields=end,counters=read_counters(folder,counter_syms),screen_sha256=sha(folder/"screen.png"))
        if example=="skinning_stress_3d":
            row["requested_actors"]=struct.unpack(">I",(folder/"actors.bin").read_bytes())[0]
            if row["requested_actors"]!=4:raise ValueError("Stress preset did not retain four actors")
        if end==CHECKPOINTS[-1]:
            profile=study(folder/"debug",elf,"sh2eb-elf-nm")
            dump(folder/"profile.json",profile)
            row["profile"]=profile_summary(profile)
        rows.append(row)
    if example=="world_lod3d_demo":
        shutil.copyfile(ROOT/"harness/scripts/world_lod3d_demo_route.pad",case/"ymir"/"route.txt")
    first,last=rows
    delta={name:last["counters"][name]-first["counters"][name] for name in COUNTERS}
    if min(delta.values())<0 or not delta["g_list_serial"]:
        raise ValueError("Non-advancing or non-monotonic scene counters")
    report=dict(checkpoints=rows,counter_deltas=delta,
         nominal_submissions_per_second=60*delta["g_list_serial"]/(last["fields"]-first["fields"]),
         scope="List submissions per nominal 60 Hz field, not physical display FPS.")
    dump(case/"ymir"/"analysis.json",report)
    return report

def compare_images(a,b,out,ignore_emit_hud=False):
    out.parent.mkdir(parents=True,exist_ok=True)
    w,h,raw=load_rgb(a);w2,h2,other=load_rgb(b)
    if (w,h)!=(w2,h2):
        return dict(status="REVIEW_RASTER",dimensions=[[w,h],[w2,h2]])
    result,mask=diff(raw,other,w,h)
    write_rgb(out,w,h,mask)
    if ignore_emit_hud and (w,h)==(320,224):
        # V3.1 exact Ymir diff bounds (248,136)-(271,143).
        # Exclude only the numeric EMIT US field, never the model or rest of HUD.
        box=(248,136,280,144)
        x0,y0,x1,y1=box
        masked=bytearray(other)
        for y in range(y0,y1):
            lo=(y*w+x0)*3;hi=(y*w+x1)*3
            masked[lo:hi]=raw[lo:hi]
        filtered,filtered_mask=diff(raw,bytes(masked),w,h)
        write_rgb(out.with_name(out.stem+"-without-emit-hud.png"),w,h,filtered_mask)
        result["outside_emit_hud"]=filtered
        result["excluded_rectangle"]=list(box)
    nonuniform=all(any(rgb[i:i+3]!=rgb[:3] for i in range(3,len(rgb),3)) for rgb in (raw,other))
    result.update(status=("REVIEW_UNIFORM" if not nonuniform else
                  "MATCH_AT_CAPTURE" if not result["different_pixels"] else "REVIEW_DYNAMIC_STATE"),
                  control_sha256=sha(a),candidate_sha256=sha(b))
    return result

def mednafen_visual(case,example,elf,bios,env,mode):
    # Use the existing real-CD/isolated HOME/autosave capture path.
    # Only panic is extracted here; wall-clock stop is not a matched
    # emulated-field interval and is unsuitable for timing or FPS.
    shared.EXAMPLE=example
    shared.DUMPS=(("g_sat_last_panic","panic.bin"),)
    shared.VISUAL_FRAMES=(900,1200)
    def parse(case,sub,mode,syms):
        validate_panic((sub/"panic.bin").read_bytes(),syms["g_sat_last_panic"][1])
        return dict(mode=mode,panic_clear=True,scope="Real CD visual smoke test, no timing claim")
    shared.parse_capture=parse
    return shared.mednafen(case,mode,env,bios,True)

def compare(a,b,case):
    reports={"ymir":{}}
    for end in CHECKPOINTS:
        reports["ymir"][str(end)]=compare_images(
            case/"control"/"ymir"/str(end)/"screen.png",
            case/"hybrid"/"ymir"/str(end)/"screen.png",
            case/"diff"/f"ymir-{end}.png",ignore_emit_hud=case.name=="skinning_stress_3d")
    before=a["nominal_submissions_per_second"];after=b["nominal_submissions_per_second"]
    reports.update(control_submissions_per_second=before,hybrid_submissions_per_second=after,
          throughput_change_percent=100*(after-before)/before,
          control_counter_deltas=a["counter_deltas"],hybrid_counter_deltas=b["counter_deltas"],
          control_profile=a["checkpoints"][-1]["profile"],hybrid_profile=b["checkpoints"][-1]["profile"])
    return reports


def fixed_pose_qualification(case,bios,probe,env,ymir_only):
    import hashlib
    from sh2_kernel_verify_mednafen import swap16
    example="skeletal_animation_3d"
    frames=(2400,3000)
    shared.EXAMPLE=example
    shared.DUMPS=(("g_hierarchy_character","telemetry.bin"),("g_sat_last_panic","panic.bin"),
                  ("g_joint_world","world.bin"),("g_mesh_vertices","vertices.bin"),
                  ("g_vdp1_pose_hashes","pose-hashes.bin"),("g_vdp1_pose_capture","pose-list.bin"),
                  ("g_vdp1_emit_mode","emit-mode.bin"))
    shared.VISUAL_FRAMES=frames
    def parse(case,sub,mode,syms):
        validate_panic((sub/"panic.bin").read_bytes(),syms["g_sat_last_panic"][1])
        if struct.unpack(">I",(sub/"emit-mode.bin").read_bytes())[0]!=mode:
            raise ValueError("Wrong VDP1 emitter mode")
        report=shared.decode((sub/"telemetry.bin").read_bytes(),1)
        for symbol,name in shared.DUMPS[2:]:
            if (sub/name).stat().st_size!=syms[symbol][1]:
                raise ValueError("Wrong pose dump size: "+name)
        hashes=struct.unpack(">48I",(sub/"pose-hashes.bin").read_bytes())
        raw=(sub/"pose-list.bin").read_bytes()
        if raw[:4]==b"DV1P":raw=swap16(raw)
        if raw[:4]!=b"VDP1" or len(raw)!=8+65536+16384:
            raise ValueError("Invalid low RAM pose capture")
        commands,gouraud=struct.unpack_from(">HH",raw,4)
        if not 1<=commands<=2048 or gouraud>2048 or commands!=report["rows"][-1]["commands"]+1:
            raise ValueError("Invalid command/gouraud count")
        payload=raw[8:8+commands*32]
        shades=raw[8+65536:8+65536+gouraud*8]
        if not (struct.unpack_from(">H",payload,len(payload)-32)[0]&0x8000):
            raise ValueError("Final command missing END")
        h=2166136261
        for byte in struct.pack(">HH",commands,gouraud)+payload+shades:
            h=((h^byte)*16777619)&0xffffffff
        if h!=hashes[-1]:raise ValueError("Pose hash does not match literal capture")
        (sub/"commands.bin").write_bytes(payload)
        (sub/"gouraud.bin").write_bytes(shades)
        report.update(emitter_mode=mode,pose_hashes=list(hashes),commands=commands,gouraud=gouraud,
            commands_sha256=hashlib.sha256(payload).hexdigest(),gouraud_sha256=hashlib.sha256(shades).hexdigest(),
            **{name+"_sha256":sha(sub/name) for name in ("world.bin","vertices.bin")})
        dump(sub/"analysis.json",report)
        return report
    shared.parse_capture=parse
    reports={}
    for variant,mode in MODES.items():
        folder=case/variant
        build(folder,example,mode,env,fixed_pose=True)
        reports[variant]={"ymir":shared.ymir(folder,mode,env,bios,probe)}
        if not ymir_only:
            reports[variant]["mednafen-visual"]=shared.mednafen(folder,mode,env,bios,True)
    result={}
    for emulator in reports["control"]:
        a,b=(reports[v][emulator] for v in MODES)
        for key in ("joints","vertices","world.bin_sha256","vertices.bin_sha256",
                    "pose_hashes","commands","gouraud","commands_sha256","gouraud_sha256"):
            if a[key]!=b[key]:raise ValueError(emulator+" fixed-pose mismatch: "+key)
        for x,y in zip(a["rows"],b["rows"]):
            for key in ("step","pose_time","matrix_hash","vertex_hash","commands","faces"):
                if x[key]!=y[key]:raise ValueError(emulator+" pose state mismatch: "+key)
        dest=case/"diff"/emulator;dest.mkdir(parents=True,exist_ok=True)
        shots=shared.YMIR_FRAMES if emulator=="ymir" else frames
        paths=lambda v,i:case/v/emulator/f"frame-{i}.png"
        stable={v:shared.pixels(paths(v,shots[0]),paths(v,shots[1]),dest/(v+"-stable.png")) for v in MODES}
        equal={str(i):shared.pixels(paths("control",i),paths("hybrid",i),dest/f"frame-{i}.png") for i in shots}
        result[emulator]=dict(passed=True,pose_rows=len(a["rows"]),command_words=a["commands"]*16,
             commands_sha256=a["commands_sha256"],gouraud_sha256=a["gouraud_sha256"],
             stable=stable,pixels=equal)
    return result

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--examples",nargs="+",choices=EXAMPLES,default=list(EXAMPLES))
    p.add_argument("--ymir-only",action="store_true")
    p.add_argument("--pose-only",action="store_true",help="Run only fixed Robot Police pose qualification")
    p.add_argument("--skip-pose",action="store_true",help="Skip separate fixed Robot Police run")
    args=p.parse_args()
    if len(set(args.examples))!=len(args.examples):p.error("duplicate examples")
    if args.pose_only and args.skip_pose:p.error("incompatible pose options")
    out=ROOT/"harness/build/vdp1_emit_real_workloads";out.mkdir(parents=True,exist_ok=True)
    run=Path(tempfile.mkdtemp(prefix="run-",dir=out))
    archive=out.parent/"vdp1_emit_real_workloads_v3_2-return.zip"
    snapshot=capture(ROOT)
    env=dict(os.environ)
    tools=Path(env.get("LIBSATURN_TOOLCHAIN_ROOT",str(Path.home()/"dev/saturn-tools")))
    env["LIBSATURN_TOOLCHAIN_ROOT"]=str(tools)
    env["PATH"]=str(tools/"bin")+os.pathsep+env.get("PATH","")
    os.environ["PATH"]=env["PATH"]
    bios=Path(env.get("LIBSATURN_BIOS",str(ROOT/"bios/firmware/mpr-17933.bin"))).resolve()
    probe=(Path(env.get("LIBSATURN_YMIR_BUILD_DIR",str(ROOT/"harness/build-macos")))/"probe").resolve()
    summary=dict(checkout=subprocess.check_output(["git","rev-parse","HEAD"],cwd=ROOT,text=True).strip(),
         completed=False,ymir_only=args.ymir_only,examples={},errors={},
         scope="Real dynamic workloads; same field does not guarantee same simulation state. No visual equivalence or promotion inferred.")
    try:
        require_ffmpeg()
        summary.update(bios_sha256=sha(bios),probe_sha256=sha(probe))
        for example in (() if args.pose_only else args.examples):
            case=run/example;case.mkdir()
            reports={}
            try:
                for variant,mode in MODES.items():
                    folder=case/variant
                    elf=build(folder,example,mode,env)
                    reports[variant]=run_ymir(folder,example,elf,probe,bios,env)
                    if not args.ymir_only:
                        mednafen_visual(folder,example,elf,bios,env,mode)
                result=compare(reports["control"],reports["hybrid"],case)
                if not args.ymir_only:
                    result["mednafen_visual"]={str(i):compare_images(
                        case/"control"/"mednafen-visual"/f"frame-{i}.png",
                        case/"hybrid"/"mednafen-visual"/f"frame-{i}.png",
                        case/"diff"/f"mednafen-{i}.png") for i in shared.VISUAL_FRAMES}
                summary["examples"][example]=result
            except Exception as exc:
                summary["errors"][example]=str(exc)
                (case/"failure.log").write_text(traceback.format_exc())
                print("FAILED:",example,exc,flush=True)
            dump(run/"summary.json",summary)
        if not args.skip_pose and (args.pose_only or "skeletal_animation_3d" in args.examples):
            case=run/"robot-fixed-pose";case.mkdir()
            try:
                summary["fixed_pose"]=fixed_pose_qualification(case,bios,probe,env,args.ymir_only)
            except Exception as exc:
                summary["errors"]["robot-fixed-pose"]=str(exc)
                (case/"failure.log").write_text(traceback.format_exc())
                print("FAILED: robot-fixed-pose",exc,flush=True)
        summary["completed"]=not summary["errors"]
    except BaseException as exc:
        summary["error"]=str(exc)
        (run/"failure.log").write_text(traceback.format_exc())
        raise
    finally:
        dump(run/"summary.json",summary)
        tmp=archive.with_suffix(".tmp.zip")
        with zipfile.ZipFile(tmp,"w",zipfile.ZIP_DEFLATED) as z:
            add_to_zip(z,snapshot)
            for path in run.rglob("*"):
                if path.is_file() and (path.suffix in (".json",".png",".log",".csv",".txt") or
                    path.name in ("panic.bin","actors.bin","telemetry.bin","world.bin","vertices.bin","pose-hashes.bin",
                                  "pose-list.bin","emit-mode.bin","commands.bin","gouraud.bin",*(name+".bin" for name in COUNTERS))):
                    z.write(path,path.relative_to(run))
        tmp.replace(archive)
        print("Return:",archive,flush=True)
    if not summary["completed"]:raise SystemExit(1)

if __name__=="__main__":main()
