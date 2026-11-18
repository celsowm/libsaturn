#!/usr/bin/env python3
"""Run the synthetic gate, then the matched three-build renderer qualification."""
import json
from pathlib import Path
import subprocess
import sys
import zipfile
from local_changes import capture,add_to_zip

ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/"harness/build"
STAGES=(("synthetic","run_near_clip_v2_bench.py","near_clip_v2_bench-return.zip"),
        ("renderer","run_near_clip_v2_renderer.py","near_clip_v2_renderer-return.zip"))

def main():
    OUT.mkdir(parents=True,exist_ok=True)
    archive=OUT/"near_clip_v2-qualification-return.zip"
    archive.unlink(missing_ok=True)
    snapshot=capture(ROOT)
    result={"completed":False,"stages":{}}
    attempted=[]
    status=1
    try:
        for name,script,filename in STAGES:
            dest=OUT/filename
            dest.unlink(missing_ok=True)
            attempted.append((name,dest))
            code=subprocess.call([sys.executable,str(ROOT/"harness/scripts"/script)],cwd=ROOT)
            result["stages"][name]={"returncode":code,"archive_present":dest.is_file()}
            if code or not dest.is_file():break
        else:
            result["completed"]=True
            status=0
    finally:
        tmp=archive.with_suffix(".tmp.zip")
        with zipfile.ZipFile(tmp,"w",zipfile.ZIP_DEFLATED) as z:
            add_to_zip(z,snapshot)
            z.writestr("qualification.json",json.dumps(result,indent=2)+"\n")
            for name,dest in attempted:
                if dest.is_file():z.write(dest,name+"/"+dest.name)
        tmp.replace(archive)
        print("Return:",archive,flush=True)
    return status

if __name__=="__main__":raise SystemExit(main())
