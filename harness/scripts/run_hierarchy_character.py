#!/usr/bin/env python3
"""Three matched Robot Police builds: fixed animation, timings, lossless images."""
import hashlib,json,os,shutil,signal,statistics,struct,subprocess,sys,tempfile,traceback,zipfile
from pathlib import Path
from cube_pipeline import ROOT,symbols,validate_panic
from sh2_kernel_verify_mednafen import elf_text,normalize,work_ram,sha
from renderer_visual import require_ffmpeg,extract_frames,load_rgb,diff,write_rgb

EXAMPLE="skeletal_animation_3d"
ROWS=48
FIELDS=("step","pose_time","sample","skeleton","hierarchy","skin","submit","flush","work",
        "matrix_hash","vertex_hash","commands","faces","draw_timeouts")
VISUAL_FRAMES=(2400,3000)
YMIR_FRAMES=(2999,3599)
DUMPS=(("g_hierarchy_character","telemetry.bin"),("g_sat_last_panic","panic.bin"),("g_joint_world","world.bin"),("g_mesh_vertices","vertices.bin"))
MODES={"control":1,"cpp":2,"assembly":3}
OUT=ROOT/"harness/build/hierarchy_character"
TIMES=("sample","skeleton","hierarchy","skin","submit","flush","work")

def dump_json(path,data):
    path.write_text(json.dumps(data,indent=2)+"\n")

def decode(raw,mode):
    if len(raw)!=32+4*ROWS*len(FIELDS):raise ValueError("Unexpected telemetry size")
    magic,ver,actual,hz,n,done,joints,vertices=struct.unpack_from(">8I",raw)
    if (magic,ver,actual,n,done)!=(0x48434331,1,mode,ROWS,1) or not hz or not 0<joints<=128 or not vertices:
        raise ValueError("Incomplete hierarchy character report")
    rows=[]
    for i in range(ROWS):
        row=dict(zip(FIELDS,struct.unpack_from(">14I",raw,32+56*i)))
        if row["step"]!=i or row["pose_time"]!=(i//2)*65536//15 or row["draw_timeouts"] or not row["faces"] or not row["commands"]:
            raise ValueError("Invalid fixed simulation row")
        if row["hierarchy"]>row["skeleton"] or row["skeleton"]>row["work"]:
            raise ValueError("Invalid timing interval")
        rows.append(row)
    for i in range(0,ROWS,2):
        for key in ("matrix_hash","vertex_hash","commands","faces"):
            if rows[i][key]!=rows[i+1][key]:raise ValueError("Repeated pose changed: "+key)
    def med(sel):
        return {key:statistics.median(r[key] for r in sel)*1e6/hz for key in TIMES}
    return dict(mode=mode,frequency=hz,joints=joints,vertices=vertices,rows=rows,
                median_us={"all":med(rows[2:]),"new_pose":med(rows[2::2]),"repeat_pose":med(rows[3::2])})

def command_run(cmd,log,env,timeout=900):
    print("Running:",*cmd[:3],flush=True)
    with log.open("w") as f:
        f.write(repr(cmd)+"\n");f.flush()
        subprocess.run(cmd,cwd=ROOT,env=env,stdout=f,stderr=subprocess.STDOUT,check=True,timeout=timeout)

def build(case,mode,env,skin=False):
    case.mkdir()
    local=dict(env,SKELETAL_ROBOT_DEMO="1",HIERARCHY_CHARACTER_BENCH="1",
               SAT_HIERARCHY_EXPERIMENT=str(1 if skin else mode),SAT_STATIC_PROJECT_EXPERIMENT="0",
               SAT_SKIN_BLEND_BENCH="0",SAT_SKIN_CHARACTER_EXPERIMENT=str(mode if skin else 0),
               SKIN_CHARACTER_BENCH=str(int(skin)))
    command_run(["sh","build-example.sh",EXAMPLE],case/"build.log",local)
    for ext in (".elf",".map",".app.bin",".iso",".bin",".cue"):
        shutil.copyfile(ROOT/"build"/(EXAMPLE+ext),case/(EXAMPLE+ext))
    elf=case/(EXAMPLE+".elf")
    syms=symbols(elf,"sh2eb-elf-nm")
    if ("sat_affine_compose_sh2" in syms)!=(not skin and mode==3) or "sat_static_project_sh2" in syms:
        raise ValueError("Unexpected experimental kernel in ELF")
    if ("g_skin_character_mode" in syms)!=skin:
        raise ValueError("Unexpected skin experiment mode symbol")
    from profile_skin_weights import inspect
    source=ROOT/"build/generated/skeletal_animation_3d/robot_police_skeletal.c"
    asset=inspect(source,"actual generated Robot Police C")
    if len(asset)!=1:raise ValueError("Expected one generated skin")
    dump_json(case/"asset-weights.json",asset)
    dump_json(case/"build.json",dict(mode=mode,elf_sha256=sha(elf),disc_sha256=sha(case/(EXAMPLE+".bin")),asset_sha256=sha(source),
         flags={k:local[k] for k in ("SKELETAL_ROBOT_DEMO","HIERARCHY_CHARACTER_BENCH","SAT_HIERARCHY_EXPERIMENT","SAT_STATIC_PROJECT_EXPERIMENT","SAT_SKIN_BLEND_BENCH","SAT_SKIN_CHARACTER_EXPERIMENT","SKIN_CHARACTER_BENCH")},
         objdump=subprocess.check_output(["sh2eb-elf-gcc","--version"],env=local,text=True).splitlines()[0]))
    # Preserve disassembly of actual evaluator and native leaf, no large ELF in ZIP.
    dis=subprocess.check_output(["sh2eb-elf-objdump","-d",str(elf)],text=True)
    (case/"disassembly.log").write_text(dis)

def data_symbols(case):
    return symbols(case/(EXAMPLE+".elf"),"sh2eb-elf-nm")

def parse_capture(case,sub,mode,syms):
    validate_panic((sub/"panic.bin").read_bytes(),syms["g_sat_last_panic"][1])
    report=decode((sub/"telemetry.bin").read_bytes(),mode)
    for symbol,name in DUMPS[2:]:
        raw=(sub/name).read_bytes()
        if len(raw)!=syms[symbol][1]:raise ValueError("Final array size mismatch: "+symbol)
        report[name+"_sha256"]=hashlib.sha256(raw).hexdigest()
    dump_json(sub/"analysis.json",report)
    return report

def ymir(case,mode,env,bios,probe):
    sub=case/"ymir";sub.mkdir()
    syms=data_symbols(case)
    cmd=[str(probe),"--iso",str(case/(EXAMPLE+".iso")),"--bin",str(case/(EXAMPLE+".app.bin")),
         "--bios",str(bios),"--boot-frames","90","--frames","3600","--sh2-cache","on",
         "--slave-reset-entry",hex(syms["saturn_slave_entry"][0]),"--out",str(sub/"probe.json"),
         "--screenshot",f"2999:{sub}/frame-2999.png","--screenshot",f"3599:{sub}/frame-3599.png",
         "--misaligned-report",str(sub/"misaligned.txt")]
    for name,target in DUMPS:
        address,size=syms[name];cmd+=["--dump-mem",f"{address:#x}:{size}:{sub/target}"]
    done=syms[DUMPS[0][0]][0]+20
    cmd+=["--screenshot-when",f"{done:#x}:1+120:{sub}/done.png"]
    command_run(cmd,sub/"run.log",env)
    # The event-triggered reference certifies that late screenshots are of
    # the final pose, rather than merely two identical earlier frames.
    pixels(sub/"done.png",sub/"frame-2999.png",sub/"done-diff.png")
    return parse_capture(case,sub,mode,syms)

def mednafen(case,mode,env,bios,visual=False):
    sub=case/("mednafen-visual" if visual else "mednafen");sub.mkdir()
    exe=shutil.which(env.get("MEDNAFEN","mednafen"),path=env["PATH"])
    if not exe:raise RuntimeError("Mednafen not on PATH")
    syms=data_symbols(case)
    elf=case/(EXAMPLE+".elf")
    with tempfile.TemporaryDirectory(prefix="med-profile-",dir=case) as tmp:
        local=dict(env,MEDNAFEN_HOME=tmp)
        cmd=[exe,"-sound","0","-video.driver","softfb","-ss.region_default","na","-ss.bios_na_eu",str(bios),
             "-autosave","1","-ss.input.port2","none"]
        for button in ("up","down","left","right","a","b","c","x","y","z","ls","rs","start"):
            cmd+=["-ss.input.port1.gamepad."+button,""]
        movie=Path(tmp)/"capture.mov"
        if visual:
            cmd+=["-ss.correct_aspect","0","-ss.h_overscan","0","-ss.slstart","0","-ss.slend","223",
                  "-qtrecord.vcodec","cscd","-qtrecord.w_double_threshold","0","-qtrecord.h_double_threshold","0","-qtrecord",str(movie)]
        cmd.append(str(case/(EXAMPLE+".cue")))
        with (sub/"run.log").open("w") as log:
            log.write(repr(cmd)+"\n");log.flush()
            p=subprocess.Popen(cmd,cwd=ROOT,env=local,stdout=log,stderr=subprocess.STDOUT)
            try:
                try:p.wait(timeout=90 if visual else 65)
                except subprocess.TimeoutExpired:
                    p.send_signal(signal.SIGINT);p.wait(timeout=20)
            finally:
                if p.poll() is None:p.kill();p.wait()
        if p.returncode:raise RuntimeError("Mednafen failed")
        states=list((Path(tmp)/"mcs").glob("*.mca"))
        if len(states)!=1:raise ValueError("Missing Mednafen autosave")
        order,high=normalize(work_ram(states[0]),*elf_text(elf))
        for name,target in DUMPS:
            address,size=syms[name];offset=address-0x06000000
            if not 0<=offset<=len(high)-size:raise ValueError("WRAM symbol out of range: "+name)
            (sub/target).write_bytes(high[offset:offset+size])
        report=parse_capture(case,sub,mode,syms)
        report.update(elf_sha256=sha(elf),bios_sha256=sha(bios),mednafen_sha256=sha(Path(exe)),wram_order=order)
        if visual:report["frames"]=extract_frames(movie,sub,VISUAL_FRAMES)
        dump_json(sub/"analysis.json",report)
        return report

def compare(a,b):
    for key in ("joints","vertices","world.bin_sha256","vertices.bin_sha256"):
        if a[key]!=b[key]:raise ValueError("Workload differs: "+key)
    for x,y in zip(a["rows"],b["rows"]):
        for key in ("step","pose_time","matrix_hash","vertex_hash","commands","faces"):
            if x[key]!=y[key]:raise ValueError(f"Simulation mismatch step {x['step']}: {key}")
    return {phase:{k:dict(control_us=x,candidate_us=b["median_us"][phase][k],
                    reduction_percent=100*(x-b["median_us"][phase][k])/x if x else None)
                   for k,x in costs.items()} for phase,costs in a["median_us"].items()}

def pixels(a,b,out):
    w,h,raw=load_rgb(a);w2,h2,other=load_rgb(b)
    if (w,h)!=(w2,h2):raise ValueError("Raster mismatch")
    result,mask=diff(raw,other,w,h)
    write_rgb(out,w,h,mask)
    result.update(control_sha256=sha(a),candidate_sha256=sha(b),dimensions=[w,h])
    if not any(raw[i:i+3]!=raw[:3] for i in range(3,len(raw),3)):
        raise ValueError("Uniform screenshot")
    if result["different_pixels"]:raise ValueError("Frozen-pose screenshots differ: "+str(out))
    return result

from local_changes import capture as capture_local_changes, add_to_zip as add_local_changes


def main(skin=False):
    out=ROOT/"harness/build/weighted_character" if skin else OUT
    modes={"control":1,"selective":2} if skin else MODES
    candidates=tuple(v for v in modes if v!="control")
    out.mkdir(parents=True,exist_ok=True)
    run=Path(tempfile.mkdtemp(prefix="run-",dir=out))
    archive=out.parent/("weighted_character-return.zip" if skin else "hierarchy_character-return.zip")
    env=dict(os.environ)
    tools=Path(env.get("LIBSATURN_TOOLCHAIN_ROOT",str(Path.home()/"dev/saturn-tools")))
    env["LIBSATURN_TOOLCHAIN_ROOT"]=str(tools)
    env["PATH"]=str(tools/"bin")+os.pathsep+env.get("PATH","")
    os.environ["PATH"]=env["PATH"]
    bios=Path(env.get("LIBSATURN_BIOS",str(ROOT/"bios/firmware/mpr-17933.bin"))).resolve()
    probe=Path(env.get("LIBSATURN_YMIR_BUILD_DIR",str(ROOT/"harness/build-macos")))/"probe"
    summary={"checkout":subprocess.check_output(["git","rev-parse","HEAD"],cwd=ROOT,text=True).strip(),
             "git_status":subprocess.check_output(["git","status","--porcelain"],cwd=ROOT,text=True),
             "completed":False,"comparisons":{}}
    local_changes=capture_local_changes(ROOT)
    try:
        require_ffmpeg()
        summary.update(bios_sha256=sha(bios),probe_sha256=sha(probe))
        reports={}
        for variant,mode in modes.items():
            case=run/variant
            build(case,mode,env,skin=skin)
            if variant!='control' and json.loads((case/'build.json').read_text())['asset_sha256']!=json.loads((run/'control/build.json').read_text())['asset_sha256']:
                raise ValueError('Generated asset changed between builds')
            reports[variant]={"ymir":ymir(case,mode,env,bios,probe),"mednafen":mednafen(case,mode,env,bios)}
            visual=mednafen(case,mode,env,bios,True)
            compare(reports[variant]["mednafen"],visual)
            compare(reports[variant]["ymir"],reports[variant]["mednafen"])
        for variant in candidates:
            summary["comparisons"][variant]={emu:compare(reports["control"][emu],reports[variant][emu])
                                              for emu in ("ymir","mednafen")}
        visual={}
        for emulator,frames in (("ymir",(2999,3599)),("mednafen-visual",VISUAL_FRAMES)):
            for variant in modes:
                folder=run/variant/emulator
                visual[emulator+"/"+variant+"/stable"]=pixels(folder/f"frame-{frames[0]}.png",
                    folder/f"frame-{frames[1]}.png",run/"visual"/emulator/variant/"stable-diff.png")
            for variant in candidates:
                for frame in frames:
                    visual[f"{emulator}/{variant}/{frame}"]=pixels(run/"control"/emulator/f"frame-{frame}.png",
                        run/variant/emulator/f"frame-{frame}.png",run/"visual"/emulator/variant/f"diff-{frame}.png")
        dump_json(run/"visual-validation.json",visual)
        summary["completed"]=True
    except BaseException as e:
        summary["error"]=str(e)
        (run/"failure.log").write_text(traceback.format_exc())
        raise
    finally:
        dump_json(run/"summary.json",summary)
        tmp=archive.with_suffix(".tmp.zip")
        with zipfile.ZipFile(tmp,"w",zipfile.ZIP_DEFLATED) as z:
            add_local_changes(z,local_changes)
            for p in run.rglob("*"):
                if p.is_file() and p.suffix in (".json",".png",".log",".txt",".bin") and p.name not in (EXAMPLE+".bin",EXAMPLE+".app.bin"):
                    z.write(p,p.relative_to(run))
        tmp.replace(archive);print("Return:",archive,flush=True)

if __name__=="__main__":main()
