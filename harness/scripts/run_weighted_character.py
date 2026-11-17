#!/usr/bin/env python3
"""Run extended B3 synthetic gate, then matched Robot Police A/B."""
import json,subprocess,sys,zipfile
from pathlib import Path
from run_hierarchy_character import main, ROOT
from local_changes import capture, add_to_zip

def run():
    out=ROOT/"harness/build"
    out.mkdir(parents=True,exist_ok=True)
    synthetic=out/"weighted_blend_bench-v3-return.zip"
    character=out/"weighted_character-return.zip"
    combined=out/"weighted_b3-qualification-return.zip"
    snapshot=capture(ROOT)
    for p in (synthetic,character,combined):p.unlink(missing_ok=True)
    status={"synthetic_returncode":None,"character_started":False,"character_completed":False}
    try:
        status["synthetic_returncode"]=subprocess.run(
            [sys.executable,str(ROOT/"harness/scripts/run_weighted_blend.py")],cwd=ROOT).returncode
        if status["synthetic_returncode"] or not synthetic.is_file():
            raise RuntimeError("Extended synthetic gate failed; inspect nested diagnostics")
        status["character_started"]=True
        main(skin=True)
        if not character.is_file():raise RuntimeError("Missing character archive")
        status["character_completed"]=True
    finally:
        with zipfile.ZipFile(combined,"w",zipfile.ZIP_DEFLATED) as z:
            add_to_zip(z,snapshot)
            z.writestr("qualification.json",json.dumps(status,indent=2)+"\n")
            for p in (synthetic,character):
                if p.is_file():z.write(p,p.name)
        print("Return:",combined,flush=True)
if __name__=="__main__":
    run()
