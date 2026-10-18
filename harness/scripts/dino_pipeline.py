#!/usr/bin/env python3
"""Retain a built Dino demo and measure a warmed interval on both SH-2s."""
import argparse,hashlib,json,shutil,struct,subprocess,sys
from pathlib import Path
from cube_pipeline import ROOT,symbols
from analyze_cpu_profile import analyze

sys.path.insert(0,str(ROOT/'harness/tests'))
from dino_runtime import analyze as runtime_gate

CONTROLS=('0 NONE\n150 START\n170 NONE\n200 A\n220 NONE\n300 A\n320 NONE\n'
          '360 R\n420 NONE\n460 X\n520 NONE\n560 C\n580 NONE\n620 Z\n640 NONE\n'
          '700 Z\n720 NONE\n800 B\n820 NONE\n')


def compare(before,after):
    from PIL import Image,ImageChops
    runs=[json.loads((p/'analysis.json').read_text()) for p in (before,after)]
    if any(run['scenario']!='paused' for run in runs):
        raise ValueError('pixel comparison requires paused controls with light disabled')
    images=[];crop=(0,56,640,200)
    for path,run in zip((before,after),runs):
        frames=run['checkpoints'][-1]['fields']
        picture=Image.open(path/f'field-{frames}.png').convert('RGB')
        if picture.size!=(640,224):raise ValueError('unexpected Dino display size')
        images.append(picture.crop(crop))
    difference=ImageChops.difference(*images);pixels=difference.tobytes()
    changed=sum(pixels[i:i+3]!=b'\0\0\0' for i in range(0,len(pixels),3))
    result=dict(before=str(before),after=str(after),model_crop=crop,different_pixels=changed,
                before_submissions=runs[0]['nominal_submissions_per_second'],
                after_submissions=runs[1]['nominal_submissions_per_second'])
    print(json.dumps(result,indent=2))
    if changed:raise ValueError('Dino fixed-pose pixels changed')


def run(a):
    out=a.out.resolve()
    if out.exists() and any(out.iterdir()):raise FileExistsError('use a fresh empty output directory')
    out.mkdir(parents=True,exist_ok=True)
    for suffix in ('.elf','.map','.app.bin','.iso','.bin','.cue'):
        shutil.copyfile(a.build/('dino_demo'+suffix),out/('dino_demo'+suffix))
    elf=out/'dino_demo.elf';syms=symbols(elf,a.nm)
    # The injection boot needs the BIOS Slave handoff supplied explicitly.
    entry=syms['saturn_slave_entry'][0]
    scenario=out/'inputs.pad'
    scenario.write_text(CONTROLS if a.scenario=='controls' else
                        '0 A+Z\n' if a.scenario=='paused' else '0 NONE\n')
    retained=dict(scenario=a.scenario,analysis_checkout=subprocess.check_output(
        ['git','describe','--always','--dirty'],cwd=ROOT,text=True).strip(),
        hashes={suffix:hashlib.sha256((out/('dino_demo'+suffix)).read_bytes()).hexdigest()
                for suffix in ('.elf','.app.bin','.bin','.cue')},
        bios_sha256=hashlib.sha256(a.bios.read_bytes()).hexdigest(),
        probe_sha256=hashlib.sha256(a.probe.read_bytes()).hexdigest(),checkpoints=[])
    for frames in a.checkpoints:
        prefix=out/('field-'+str(frames))
        shutil.copyfile(out/'dino_demo.map',Path(str(prefix)+'.map'))
        command=[str(a.probe.resolve()),'--iso',str(out/'dino_demo.iso'),
                 '--bin',str(out/'dino_demo.app.bin'),'--bios',str(a.bios.resolve()),
                 '--slave-reset-entry',hex(entry),'--sh2-cache','on',
                 '--boot-frames','90','--frames',str(frames),'--pad-script',str(scenario),
                 '--dump-wram-high',str(prefix)+'_high.bin','--out',str(prefix)+'.json',
                 '--screenshot',f'{frames-1}:{prefix}.png']
        for name in ('g_anim','g_light_on'):
            address,size=syms[name]
            command+=['--dump-mem',f'{address:#x}:{size}:{prefix}_{name}.bin']
        if a.profile and frames==a.checkpoints[-1]:
            command+=['--debug-profile',str(prefix)+'_profile','--trace-from',str(frames-40),
                      '--trace-frames','12','--trace-capacity','524288']
        Path(str(prefix)+'_command.json').write_text(json.dumps(command,indent=2)+'\n')
        with Path(str(prefix)+'_run.log').open('w') as log:
            subprocess.run(command,cwd=ROOT,stdout=log,stderr=subprocess.STDOUT,
                           timeout=a.timeout,check=True)
        runtime_gate(prefix)
        metrics=json.loads(Path(str(prefix)+'_analysis.json').read_text())['metrics']
        clip,pose,flags,reserved,time=struct.unpack('>4Hi',Path(str(prefix)+'_g_anim.bin').read_bytes())
        light=struct.unpack('>I',Path(str(prefix)+'_g_light_on.bin').read_bytes())[0]
        if a.scenario=='paused' and (not flags&1 or time or light):
            raise ValueError('paused control must retain time zero and disable light')
        retained['checkpoints'].append(dict(fields=frames,metrics=metrics,
            animation=dict(clip=clip,pose=pose,flags=flags,time=time),light=light))
        if a.profile and frames==a.checkpoints[-1]:
            profile=analyze(str(prefix)+'_profile',elf,a.nm)
            meta=profile['profile']
            if meta['dropped_frames'] or any(meta[cpu][key] for cpu in ('master','slave')
                    for key in ('dropped_pc_observations','dropped_events','dropped_snapshots')):
                raise ValueError('incomplete Dino CPU capture')
            Path(str(prefix)+'_profile_summary.json').write_text(json.dumps(profile,indent=2)+'\n')
    first,last=retained['checkpoints'][0],retained['checkpoints'][-1]
    delta=last['metrics']['frames']-first['metrics']['frames']
    if delta<=0:raise ValueError('non-advancing Dino completed frames')
    retained['nominal_submissions_per_second']=delta*60/(last['fields']-first['fields'])
    retained['scope']='Completed CPU submissions over warmed nominal 60 Hz Ymir fields; not distinct presentation or physical hardware timings.'
    (out/'analysis.json').write_text(json.dumps(retained,indent=2)+'\n')
    print(f"Dino {a.scenario}: {retained['nominal_submissions_per_second']:.3f} submissions/s",flush=True)


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--build',type=Path,default=ROOT/'build')
    p.add_argument('--out',type=Path);p.add_argument('--bios',type=Path)
    p.add_argument('--compare',type=Path,nargs=2,metavar=('BEFORE','AFTER'),
                   help='compare retained fixed-pose images and report cadence (requires Pillow)')
    p.add_argument('--probe',type=Path,default=ROOT/'harness/build-macos/probe')
    p.add_argument('--nm',default=str(Path.home()/'dev/saturn-tools/bin/sh2eb-elf-nm'))
    p.add_argument('--checkpoints',type=int,nargs='+',default=[600,1200])
    p.add_argument('--scenario',choices=['animated','paused','controls'],default='animated')
    p.add_argument('--profile',action='store_true');p.add_argument('--timeout',type=int,default=600)
    a=p.parse_args()
    if a.compare:
        compare(*a.compare);raise SystemExit(0)
    if not a.out or not a.bios:p.error('--out and --bios are required for execution')
    if len(a.checkpoints)<2 or a.checkpoints!=sorted(set(a.checkpoints)) or a.checkpoints[0]<100:
        p.error('need at least two increasing checkpoints >=100 fields')
    run(a)
