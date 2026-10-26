#!/usr/bin/env python3
"""Build and retain an SDK example under its shared-alias lock."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess

from cube_pipeline import ROOT,build_lock,make_program

ARTIFACTS=('.elf','.map','.app.bin','.iso','.bin','.cue')


def build(example,out,options,compiler_dir,python,timeout=600):
    if not re.fullmatch(r'[a-zA-Z0-9_]+',example):raise ValueError('invalid example name')
    if not (ROOT/'examples'/example/'main.c').is_file():raise ValueError('missing example main.c')
    keys=[]
    for option in options:
        if not re.fullmatch(r'[A-Z][A-Z0-9_]*=[^\x00\r\n]+',option):
            raise ValueError('make options must be NAME=value')
        keys.append(option.split('=',1)[0])
    if len(set(keys))!=len(keys) or {'EXAMPLE','PYTHON','BUILD_DIR','GENERATED_DIR'} & set(keys):
        raise ValueError('duplicate or reserved make option')
    out=out.resolve()
    if out.exists() and any(out.iterdir()):raise FileExistsError('use a fresh output directory')
    out.mkdir(parents=True,exist_ok=True)
    env=dict(os.environ,LIBSATURN_EXAMPLE_LOCKED='1')
    env['PATH']=str(compiler_dir.resolve())+os.pathsep+env.get('PATH','')
    # Resolving the interpreter symlink discards its venv and dependencies.
    command=[make_program(),'all','EXAMPLE='+example,'PYTHON='+str(python.absolute()),*options,'-j4']
    manifest=dict(example=example,command=command,completed=False,
        revision=subprocess.check_output(['git','describe','--always','--dirty'],cwd=ROOT,text=True).strip(),
        source_hashes={str(path.relative_to(ROOT)):hashlib.sha256(path.read_bytes()).hexdigest()
            for path in (ROOT/'Makefile',ROOT/'examples'/example/'main.c',
                         ROOT/'examples'/example/'Makefile.inc') if path.is_file()})
    (out/'variant.json').write_text(json.dumps(manifest,indent=2)+'\n')
    with build_lock(example):
        with (out/'build.log').open('w') as log:
            subprocess.run(command,cwd=ROOT,env=env,stdout=log,stderr=subprocess.STDOUT,
                           check=True,timeout=timeout)
        for suffix in ARTIFACTS:
            shutil.copyfile(ROOT/'build'/(example+suffix),out/(example+suffix))
    manifest['hashes']={suffix:hashlib.sha256((out/(example+suffix)).read_bytes()).hexdigest()
                        for suffix in ARTIFACTS}
    for name,digest in manifest['source_hashes'].items():
        if hashlib.sha256((ROOT/name).read_bytes()).hexdigest()!=digest:
            raise ValueError('build source changed during compilation: '+name)
    manifest['completed']=True
    (out/'variant.json').write_text(json.dumps(manifest,indent=2)+'\n')
    print('Retained '+example+': '+str(out),flush=True)


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--example',required=True)
    p.add_argument('--out',type=Path,required=True)
    p.add_argument('--compiler-dir',type=Path,required=True)
    p.add_argument('--python',type=Path,required=True)
    p.add_argument('--make-option',action='append',default=[])
    p.add_argument('--timeout',type=int,default=600)
    a=p.parse_args();build(a.example,a.out,a.make_option,a.compiler_dir,a.python,a.timeout)
