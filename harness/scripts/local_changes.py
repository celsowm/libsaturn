#!/usr/bin/env python3
"""Read-only source review evidence for benchmark return archives.

Capture before building, so local edits can be reviewed independently of
benchmark-generated files. Never stage, stash, reset, commit or push.
Tracked text patches may contain sensitive information; inspect before sharing.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import zipfile

SOURCE_ROOTS={"src","include","examples","tests","harness","tools","cmake","docs"}
SOURCE_SUFFIXES={".c",".cpp",".cc",".cxx",".h",".hpp",".s",".S",".asm",".inc",
                 ".py",".sh",".ps1",".cmake",".md"}
SKIP_PARTS={"build","generated","gen","assets","bios","vendor","third_party","node_modules","__pycache__"}
MAX_FILE=256*1024
MAX_TOTAL=2*1024*1024


def git(root,*args):
    return subprocess.check_output(["git","--no-optional-locks","-c","core.quotePath=true",*args],
                                   cwd=root,stderr=subprocess.PIPE)


def capture(root):
    """Return archive members with relative names, captured at call time."""
    root=Path(root).resolve()
    members={}
    try:
        head=git(root,"rev-parse","HEAD").decode().strip()
        members["head.txt"]=(head+"\n").encode()
        members["status.txt"]=git(root,"status","--short","--branch","--untracked-files=all")
        opts=("--no-ext-diff","--no-textconv","--no-color","--full-index")
        # HEAD diff shows the resulting working tree. Separate patches also
        # preserve staged changes that were undone in the working tree.
        members["tracked.patch"]=git(root,"diff",*opts,"HEAD","--")
        members["staged.patch"]=git(root,"diff",*opts,"--cached","HEAD","--")
        members["unstaged.patch"]=git(root,"diff",*opts,"--")
        members["diff-stat.txt"]=git(root,"diff","--no-ext-diff","--stat","HEAD","--")
        paths=git(root,"ls-files","--others","--exclude-standard","-z").split(b"\0")
        manifest={"head":head,"complete":True,"tracked_binary_contents_included":False,
                  "untracked_policy":"Only bounded UTF-8 source files in known source directories; all others listed, not copied.",
                  "untracked":[]}
        used=0
        for item in paths:
            if not item:continue
            name=os.fsdecode(item)
            p=Path(name)
            row={"path":name,"included":False}
            manifest["untracked"].append(row)
            if (len(p.parts)<2 or p.parts[0] not in SOURCE_ROOTS or
                p.suffix not in SOURCE_SUFFIXES or
                any(part.startswith(".") or part in SKIP_PARTS or
                    any(word in part.lower() for word in ("secret","credential","password","private_key","token"))
                    for part in p.parts)):
                row["reason"]="outside source allowlist";continue
            target=root/p
            if (not target.is_file() or target.is_symlink() or
                any(parent.is_symlink() for parent in target.parents if parent!=root and root in parent.parents) or
                not target.resolve().is_relative_to(root)):
                row["reason"]="not an ordinary repository file";continue
            size=target.stat().st_size
            row["size"]=size
            if size>MAX_FILE or used+size>MAX_TOTAL:
                row["reason"]="size limit";continue
            raw=target.read_bytes()
            if len(raw)>MAX_FILE or used+len(raw)>MAX_TOTAL or b"\0" in raw:
                row["reason"]="size limit or binary";continue
            try:raw.decode("utf-8")
            except UnicodeDecodeError:
                row["reason"]="not UTF-8";continue
            members["untracked/"+name]=raw
            row.update(included=True,sha256=hashlib.sha256(raw).hexdigest(),size=len(raw))
            used+=len(raw)
        manifest["included_untracked_bytes"]=used
        members["manifest.json"]=(json.dumps(manifest,indent=2,ensure_ascii=True)+"\n").encode()
    except (OSError,subprocess.CalledProcessError) as exc:
        members["manifest.json"]=(json.dumps({"complete":False,"error":str(exc)},indent=2)+"\n").encode()
        print("Local source review capture incomplete:",exc,flush=True)
    return members


def add_to_zip(archive,members):
    for name,raw in members.items():
        archive.writestr("local-changes/"+name,raw)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out",type=Path,required=True)
    args=parser.parse_args()
    root=Path(__file__).resolve().parents[2]
    members=capture(root)
    args.out.parent.mkdir(parents=True,exist_ok=True)
    with zipfile.ZipFile(args.out,"w",zipfile.ZIP_DEFLATED) as z:
        add_to_zip(z,members)
    print("Return:",args.out)
    return 0 if json.loads(members["manifest.json"])["complete"] else 1


if __name__=="__main__":
    raise SystemExit(main())
