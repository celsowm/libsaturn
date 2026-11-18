#!/usr/bin/env python3
"""Matched world-cull renderer A/B, exact VDP1 bytes plus lossless images."""
import json,hashlib,os,shutil,statistics,struct,subprocess,sys,tempfile,traceback,zipfile
from pathlib import Path
import run_hierarchy_character as shared
from cube_pipeline import ROOT,symbols,validate_panic
from sh2_kernel_verify_mednafen import sha
from local_changes import capture,add_to_zip
from renderer_visual import require_ffmpeg

EXAMPLE="world_cull_renderer"
STEPS=16
CAP=256
GCAP=32
FIELDS=("step","pose","path","submit","projection","prepare","flush","end","commands","gouraud","culled","faces","timeouts","serial")
CENSUS=("input","fast_instances","general_instances","bypassed","screen_reject","world_tests","small","general16","wide","degenerate","world_reject","zero_normal")
DUMPS=(("g_world_cull_renderer","telemetry.bin"),("g_sat_last_panic","panic.bin"),
       ("g_world_cull_commands","commands.bin"),("g_world_cull_gouraud","gouraud.bin"))
MODES={"control":1,"wide":2,"census":3}

def dump(path,data):
    shared.dump_json(path,data)

PHASES=("general","compiled")
TIMES=("submit","projection","prepare","flush","end")
def medians(rows,hz):
    costs={key:statistics.median(r[key] for r in rows)*1e6/hz for key in TIMES}
    costs["submit_plus_flush"]=statistics.median(r["submit"]+r["flush"] for r in rows)*1e6/hz
    return costs

def parse_capture(case,sub,mode,syms):
    validate_panic((sub/"panic.bin").read_bytes(),syms["g_sat_last_panic"][1])
    raw=(sub/"telemetry.bin").read_bytes()
    if len(raw)!=32+STEPS*104:raise ValueError("Wrong world-cull scene report size")
    magic,ver,actual,hz,count,done,faces,verts=struct.unpack_from(">8I",raw)
    if (magic,ver,actual,count,done)!=(0x57435231,1,mode,STEPS,1) or not hz or not 16<=faces<=200 or not 0<verts<=320:
        raise ValueError("Incomplete world-cull scene report")
    commands=(sub/"commands.bin").read_bytes()
    gouraud=(sub/"gouraud.bin").read_bytes()
    if len(commands)!=STEPS*CAP*32 or len(gouraud)!=STEPS*GCAP*8:
        raise ValueError("Unexpected capture layout")
    rows=[]
    for i in range(STEPS):
        values=struct.unpack_from(">26I",raw,32+i*104)
        row=dict(zip(FIELDS,values[:14]))
        counts=dict(zip(CENSUS,values[14:]))
        if (row["step"],row["pose"],row["path"])!=(i,(i//2)%4,i//8) or row["timeouts"]:
            raise ValueError("Invalid scene step")
        if not 2<row["commands"]<=CAP or not 0<row["gouraud"]<=GCAP or not row["faces"] or not row["prepare"] or not row["projection"]:
            raise ValueError("Missing rendered geometry / profiling")
        if i and row["serial"]!=rows[-1]["serial"]+1:raise ValueError("Unexpected list serial")
        if mode==3:
            if (counts["input"],counts["fast_instances"],counts["general_instances"])!=(faces,i//8,1-i//8):
                raise ValueError(f"Requested scene path not exercised: {i} {counts}")
            if counts["bypassed"]!=(faces+16)//17:
                raise ValueError("Unexpected double-sided count")
            if counts["input"]!=counts["bypassed"]+counts["screen_reject"]+counts["world_tests"]:
                raise ValueError("Incomplete world cull census")
            if counts["world_tests"]!=counts["small"]+counts["general16"]+counts["wide"]:
                raise ValueError("Incomplete kernel route census")
            if not 0<=counts["zero_normal"]<=counts["degenerate"]<=counts["wide"] or counts["world_reject"]>counts["world_tests"]:
                raise ValueError("Inconsistent fallback count")
        elif any(counts.values()):
            raise ValueError("Census enabled in timing build")
        cb=commands[i*CAP*32:(i+1)*CAP*32]
        gb=gouraud[i*GCAP*8:(i+1)*GCAP*8]
        used=row["commands"]*32;gu=row["gouraud"]*8
        if not struct.unpack_from(">H",cb,used-32)[0]&0x8000:
            raise ValueError("Missing END command")
        if any(cb[used:]) or any(gb[gu:]):raise ValueError("Nonzero capture tail")
        row.update(census=counts,phase=PHASES[i//8],
                   commands_sha256=hashlib.sha256(cb[:used]).hexdigest(),
                   gouraud_sha256=hashlib.sha256(gb[:gu]).hexdigest())
        rows.append(row)
    report=dict(mode=mode,frequency=hz,mesh_faces=faces,vertices=verts,rows=rows,
                commands_sha256=sha(sub/"commands.bin"),gouraud_sha256=sha(sub/"gouraud.bin"))
    # No timing inference from the extra per-face census code.
    if mode!=3:
        report["phase_median_us"]={p:medians([r for r in rows if r["phase"]==p][2:],hz) for p in PHASES}
    else:
        report["phase_census"]={p:{k:sum(r["census"][k] for r in rows if r["phase"]==p) for k in CENSUS} for p in PHASES}
    dump(sub/"analysis.json",report)
    return report

def compare(a,b):
    for key in ("mesh_faces","vertices","commands_sha256","gouraud_sha256"):
        if a[key]!=b[key]:raise ValueError("Literal capture differs: "+key)
    for x,y in zip(a["rows"],b["rows"]):
        for key in ("step","pose","path","commands","gouraud","culled","faces","serial","commands_sha256","gouraud_sha256"):
            if x[key]!=y[key]:raise ValueError(f"Step {x['step']} differs: {key}")
        if a["mode"]==b["mode"]==3 and x["census"]!=y["census"]:
            raise ValueError("Census differs across captures")
    if "phase_median_us" not in a or "phase_median_us" not in b:return {"exact":True}
    return {p:{key:dict(control_us=v,candidate_us=b["phase_median_us"][p][key],
                       reduction_percent=100*(v-b["phase_median_us"][p][key])/v if v else None)
               for key,v in a["phase_median_us"][p].items()} for p in PHASES}

def build(case,mode,env):
    case.mkdir()
    flags={"SAT_WORLD_CULL_EXPERIMENT":str(mode),"SAT_PROFILE_METRICS":"1","SAT_NEAR_CLIP_EXPERIMENT":"0","SAT_SCREEN_CLIP_EXPERIMENT":"0","SAT_STATIC_PROJECT_EXPERIMENT":"0",
           "SAT_HIERARCHY_EXPERIMENT":"0","SAT_SKIN_BLEND_BENCH":"0","SAT_SKIN_CHARACTER_EXPERIMENT":"0"}
    local=dict(env,**flags)
    shared.command_run(["sh","build-example.sh",EXAMPLE],case/"build.log",local)
    for ext in (".elf",".map",".app.bin",".iso",".bin",".cue"):
        shutil.copyfile(ROOT/"build"/(EXAMPLE+ext),case/(EXAMPLE+ext))
    elf=case/(EXAMPLE+".elf")
    syms=symbols(elf,"sh2eb-elf-nm")
    if "g_world_cull_mode" not in syms or any(k in syms for k in ("sat_affine_compose_sh2","sat_static_project_sh2","g_skin_character_mode","g_screen_clip_mode","g_near_clip_mode")):
        raise ValueError("Unexpected experimental kernel selection")
    dump(case/"build.json",dict(mode=mode,flags=flags,asset_sha256=sha(ROOT/"build/generated"/EXAMPLE/"sonic_model.c"),elf_sha256=sha(elf),disc_sha256=sha(case/(EXAMPLE+".bin")),
        compiler=subprocess.check_output(["sh2eb-elf-gcc","--version"],env=local,text=True).splitlines()[0]))
    (case/"disassembly.log").write_text(subprocess.check_output(["sh2eb-elf-objdump","-d",str(elf)],text=True))

def main():
    # Reuse the already qualified capture backend, with an explicit scene
    # schema and symbol list. No change to the hierarchy/skin entry points.
    shared.EXAMPLE=EXAMPLE
    shared.DUMPS=DUMPS
    shared.parse_capture=parse_capture
    out=ROOT/"harness/build/world_cull_renderer"
    out.mkdir(parents=True,exist_ok=True)
    run=Path(tempfile.mkdtemp(prefix="run-",dir=out))
    archive=out.parent/"world_cull_renderer-return.zip"
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
            if variant!="control":
                if json.loads((case/"build.json").read_text())["asset_sha256"]!=json.loads((run/"control/build.json").read_text())["asset_sha256"]:
                    raise ValueError("Generated model changed")
            reports[variant]={"ymir":shared.ymir(case,mode,env,bios,probe),
                              "mednafen":shared.mednafen(case,mode,env,bios),
                              "mednafen-visual":shared.mednafen(case,mode,env,bios,True)}
            compare(reports[variant]["mednafen"],reports[variant]["mednafen-visual"])
            compare(reports[variant]["ymir"],reports[variant]["mednafen"])
        for emulator in ("ymir","mednafen","mednafen-visual"):
            summary["comparisons"][emulator]=compare(reports["control"][emulator],reports["wide"][emulator])
            compare(reports["control"][emulator],reports["census"][emulator])
        summary["census"]=reports["census"]["mednafen"]["phase_census"]
        visual={}
        for emulator,frames in (("ymir",shared.YMIR_FRAMES),("mednafen-visual",shared.VISUAL_FRAMES)):
            for variant in MODES:
                folder=run/variant/emulator
                visual[f"{emulator}/{variant}/stable"]=shared.pixels(folder/f"frame-{frames[0]}.png",
                    folder/f"frame-{frames[1]}.png",run/"visual"/emulator/variant/"stable-diff.png")
            for variant in ("wide","census"):
                for frame in frames:
                    visual[f"{emulator}/{variant}/{frame}"]=shared.pixels(run/"control"/emulator/f"frame-{frame}.png",
                        run/variant/emulator/f"frame-{frame}.png",run/"visual"/emulator/variant/f"diff-{frame}.png")
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
