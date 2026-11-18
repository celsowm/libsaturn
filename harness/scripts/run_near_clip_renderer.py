#!/usr/bin/env python3
"""Matched near-plane renderer A/B, exact VDP1 bytes plus lossless images."""
import json,hashlib,os,shutil,statistics,struct,subprocess,sys,tempfile,traceback,zipfile
from pathlib import Path
import run_hierarchy_character as shared
from cube_pipeline import ROOT,symbols,validate_panic
from sh2_kernel_verify_mednafen import sha
from local_changes import capture,add_to_zip
from renderer_visual import require_ffmpeg

EXAMPLE="near_clip_renderer"
STEPS=24
CAP=256
GCAP=256
FIELDS=("step","pose","clip_ticks","calls","multi_piece","pieces","submit_ticks","end_ticks","commands","gouraud","draw_timeouts","serial")
DUMPS=(("g_near_clip_renderer","telemetry.bin"),("g_sat_last_panic","panic.bin"),
       ("g_near_clip_commands","commands.bin"),("g_near_clip_gouraud","gouraud.bin"))
MODES={"control":1,"bounded":2}

def dump(path,data):
    shared.dump_json(path,data)

PHASES=("axial_crossing","oblique_inside","oblique_crossing")
def phase_for(pose):
    return PHASES[pose//4]

def medians(rows,hz):
    costs={key:statistics.median(r[key] for r in rows)*1e6/hz
           for key in ("clip_ticks","submit_ticks","end_ticks")}
    costs["submit_plus_end"]=statistics.median(r["submit_ticks"]+r["end_ticks"] for r in rows)*1e6/hz
    return costs

def parse_capture(case,sub,mode,syms):
    validate_panic((sub/"panic.bin").read_bytes(),syms["g_sat_last_panic"][1])
    raw=(sub/"telemetry.bin").read_bytes()
    if len(raw)!=32+STEPS*48:raise ValueError("Wrong scene telemetry size")
    magic,ver,actual,hz,count,done,cap,gcap=struct.unpack_from(">8I",raw)
    if (magic,ver,actual,count,done,cap,gcap)!=(0x4e435231,1,mode,STEPS,1,CAP,GCAP) or not hz:
        raise ValueError("Incomplete or unexpected near-plane scene report")
    commands=(sub/"commands.bin").read_bytes()
    gouraud=(sub/"gouraud.bin").read_bytes()
    if len(commands)!=STEPS*CAP*32 or len(gouraud)!=STEPS*GCAP*8:
        raise ValueError("Unexpected command capture layout")
    rows=[]
    for i in range(STEPS):
        row=dict(zip(FIELDS,struct.unpack_from(">12I",raw,32+i*48)))
        if row["step"]!=i or row["pose"]!=i//2 or row["draw_timeouts"]:
            raise ValueError("Invalid or timed-out scene step")
        phase=phase_for(row["pose"])
        crossing=phase!="oblique_inside"
        # 18 quads with two near-inside corners yield two fan pieces
        # each. Ten other faces are entirely inside; one is rejected.
        if (row["calls"],row["multi_piece"],row["pieces"])!=(29,18 if crossing else 0,46 if crossing else 28):
            raise ValueError(f"Wrong near-plane incidence at step {i}: {row}")
        if not row["gouraud"] or not 2<row["commands"]<=CAP or row["gouraud"]>GCAP:
            raise ValueError("Missing emitted/Gouraud workload")
        row["phase"]=phase
        if i and row["serial"]!=rows[-1]["serial"]+1:
            raise ValueError("Unexpected submitted list serial")
        cb=commands[i*CAP*32:(i+1)*CAP*32]
        gb=gouraud[i*GCAP*8:(i+1)*GCAP*8]
        used=row["commands"]*32
        gu=row["gouraud"]*8
        if not struct.unpack_from(">H",cb,used-32)[0]&0x8000:
            raise ValueError("Missing VDP1 END command")
        if any(cb[used:]) or any(gb[gu:]):
            raise ValueError("Nonzero capture tail")
        row["commands_sha256"]=hashlib.sha256(cb[:used]).hexdigest()
        row["gouraud_sha256"]=hashlib.sha256(gb[:gu]).hexdigest()
        rows.append(row)
    # Bank-relative addresses in raw tables can change across alternating
    # frames. Compare each step against the same step of the other build.
    costs=medians(rows[2:],hz)
    phase_costs={phase:medians([r for r in rows if r["phase"]==phase][2:],hz)
                 for phase in PHASES}
    report=dict(mode=mode,frequency=hz,rows=rows,median_us=costs,phase_median_us=phase_costs,
        commands_sha256=sha(sub/"commands.bin"),gouraud_sha256=sha(sub/"gouraud.bin"))
    dump(sub/"analysis.json",report)
    return report

def compare(a,b):
    for key in ("commands_sha256","gouraud_sha256"):
        if a[key]!=b[key]:raise ValueError("Literal VDP1 capture differs: "+key)
    for x,y in zip(a["rows"],b["rows"]):
        for key in ("step","pose","phase","calls","multi_piece","pieces","commands","gouraud","serial","commands_sha256","gouraud_sha256"):
            if x[key]!=y[key]:raise ValueError(f"Step {x['step']} differs: {key}")
    def timing(x,y):
        return {key:dict(control_us=v,candidate_us=y[key],
                     reduction_percent=100*(v-y[key])/v if v else None)
                for key,v in x.items()}
    return dict(all=timing(a["median_us"],b["median_us"]),
        phases={phase:timing(a["phase_median_us"][phase],b["phase_median_us"][phase])
                for phase in PHASES})

def build(case,mode,env):
    case.mkdir()
    flags={"SAT_NEAR_CLIP_EXPERIMENT":str(mode),"SAT_SCREEN_CLIP_EXPERIMENT":"0","SAT_STATIC_PROJECT_EXPERIMENT":"0",
           "SAT_HIERARCHY_EXPERIMENT":"0","SAT_SKIN_BLEND_BENCH":"0","SAT_SKIN_CHARACTER_EXPERIMENT":"0"}
    local=dict(env,**flags)
    shared.command_run(["sh","build-example.sh",EXAMPLE],case/"build.log",local)
    for ext in (".elf",".map",".app.bin",".iso",".bin",".cue"):
        shutil.copyfile(ROOT/"build"/(EXAMPLE+ext),case/(EXAMPLE+ext))
    elf=case/(EXAMPLE+".elf")
    syms=symbols(elf,"sh2eb-elf-nm")
    if "g_near_clip_mode" not in syms or any(k in syms for k in ("sat_affine_compose_sh2","sat_static_project_sh2","g_skin_character_mode","g_screen_clip_mode")):
        raise ValueError("Unexpected experimental kernel selection")
    dump(case/"build.json",dict(mode=mode,flags=flags,elf_sha256=sha(elf),disc_sha256=sha(case/(EXAMPLE+".bin")),
        compiler=subprocess.check_output(["sh2eb-elf-gcc","--version"],env=local,text=True).splitlines()[0]))
    (case/"disassembly.log").write_text(subprocess.check_output(["sh2eb-elf-objdump","-d",str(elf)],text=True))

def main():
    # Reuse the already qualified capture backend, with an explicit scene
    # schema and symbol list. No change to the hierarchy/skin entry points.
    shared.EXAMPLE=EXAMPLE
    shared.DUMPS=DUMPS
    shared.parse_capture=parse_capture
    out=ROOT/"harness/build/near_clip_renderer"
    out.mkdir(parents=True,exist_ok=True)
    run=Path(tempfile.mkdtemp(prefix="run-",dir=out))
    archive=out.parent/"near_clip_renderer-return.zip"
    snapshot=capture(ROOT)
    env=dict(os.environ)
    tools=Path(env.get("LIBSATURN_TOOLCHAIN_ROOT",str(Path.home()/"dev/saturn-tools")))
    env["LIBSATURN_TOOLCHAIN_ROOT"]=str(tools)
    env["PATH"]=str(tools/"bin")+os.pathsep+env.get("PATH","")
    os.environ["PATH"]=env["PATH"]
    bios=Path(env.get("LIBSATURN_BIOS",str(ROOT/"bios/firmware/mpr-17933.bin"))).resolve()
    probe=Path(env.get("LIBSATURN_YMIR_BUILD_DIR",str(ROOT/"harness/build-macos")))/"probe"
    summary={"checkout":subprocess.check_output(["git","rev-parse","HEAD"],cwd=ROOT,text=True).strip(),
             "completed":False,"comparisons":{}}
    try:
        require_ffmpeg()
        summary.update(bios_sha256=sha(bios),probe_sha256=sha(probe))
        reports={}
        for variant,mode in MODES.items():
            case=run/variant
            build(case,mode,env)
            reports[variant]={"ymir":shared.ymir(case,mode,env,bios,probe),
                              "mednafen":shared.mednafen(case,mode,env,bios),
                              "mednafen-visual":shared.mednafen(case,mode,env,bios,True)}
            compare(reports[variant]["mednafen"],reports[variant]["mednafen-visual"])
            compare(reports[variant]["ymir"],reports[variant]["mednafen"])
        for emulator in ("ymir","mednafen","mednafen-visual"):
            summary["comparisons"][emulator]=compare(reports["control"][emulator],reports["bounded"][emulator])
        visual={}
        for emulator,frames in (("ymir",shared.YMIR_FRAMES),("mednafen-visual",shared.VISUAL_FRAMES)):
            for variant in MODES:
                folder=run/variant/emulator
                visual[f"{emulator}/{variant}/stable"]=shared.pixels(folder/f"frame-{frames[0]}.png",
                    folder/f"frame-{frames[1]}.png",run/"visual"/emulator/variant/"stable-diff.png")
            for frame in frames:
                visual[f"{emulator}/{frame}"]=shared.pixels(run/"control"/emulator/f"frame-{frame}.png",
                    run/"bounded"/emulator/f"frame-{frame}.png",run/"visual"/emulator/f"diff-{frame}.png")
        dump(run/"visual-validation.json",visual)
        summary["completed"]=True
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
                if path.is_file() and path.suffix in (".json",".png",".log",".txt",".bin") and path.name not in (EXAMPLE+".bin",EXAMPLE+".app.bin"):
                    z.write(path,path.relative_to(run))
        tmp.replace(archive)
        print("Return:",archive,flush=True)
if __name__=="__main__":main()
