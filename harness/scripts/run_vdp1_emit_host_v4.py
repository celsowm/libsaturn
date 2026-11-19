#!/usr/bin/env python3
"""C3 V4 host control/candidate literal regression; independent build dirs."""
import hashlib
import json
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
import zipfile

ROOT=Path(__file__).resolve().parents[2]

def main():
    out=ROOT/"harness/build";out.mkdir(parents=True,exist_ok=True)
    run=Path(tempfile.mkdtemp(prefix="vdp1-host-v4-",dir=out))
    result={"completed":False,"checkout":subprocess.check_output(["git","rev-parse","HEAD"],cwd=ROOT,text=True).strip(),"modes":{}}
    compiler=shlex.split(os.environ.get("HOST_CXX","c++"))
    try:
        for mode in (1,3):
            folder=run/str(mode);folder.mkdir()
            for name in ("test_vdp1_clip","test_vdp1_checkpoint","test_vdp1_banks"):
                exe=folder/name
                cmd=compiler+["-std=c++20","-O1","-Wall","-Wextra","-Iinclude","-iquote",".",
                    "-Iexamples/common",f"-DSAT_VDP1_EMIT_EXPERIMENT={mode}",
                    "-DSAT_NATIVE_SCENE_COMMANDS="+str(int(name!="test_vdp1_clip")),
                    "tests/host/"+name+".cpp","src/hal/vdp1/vdp1.cpp",
                    "tests/host_stubs/stub_scu_dma.cpp"]
                if name!="test_vdp1_banks":cmd+=["tests/host_stubs/stub_vdp1_clock.cpp"]
                cmd+=["-o",str(exe)]
                with (folder/(name+".log")).open("w") as log:
                    log.write(repr(cmd)+"\n");log.flush()
                    subprocess.run(cmd,cwd=ROOT,stdout=log,stderr=subprocess.STDOUT,check=True)
                    subprocess.run([str(exe)]+([str(folder/"commands.bin")] if name=="test_vdp1_clip" else []),
                                   cwd=ROOT,stdout=log,stderr=subprocess.STDOUT,check=True)
            raw=(folder/"commands.bin").read_bytes()
            if len(raw)!=2048*32:raise ValueError("Unexpected command capture size")
            result["modes"][str(mode)]={"commands_sha256":hashlib.sha256(raw).hexdigest(),"tests":3}
        if (run/"1/commands.bin").read_bytes()!=(run/"3/commands.bin").read_bytes():
            raise ValueError("Host command records differ")
        result["completed"]=True
    finally:
        (run/"summary.json").write_text(json.dumps(result,indent=2)+"\n")
        with zipfile.ZipFile(out/"vdp1_emit_host_v4-return.zip","w",zipfile.ZIP_DEFLATED) as z:
            for path in run.rglob("*"):
                if path.is_file() and path.suffix in (".json",".log",".bin"):
                    z.write(path,path.relative_to(run))
        print("Return:",out/"vdp1_emit_host_v4-return.zip",flush=True)

if __name__=="__main__":main()
