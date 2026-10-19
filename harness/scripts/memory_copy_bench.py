#!/usr/bin/env python3
"""Capture and compare guarded normal-RAM memcpy measurements on SH-2."""
import argparse,hashlib,json,os,shutil,statistics,struct,subprocess,sys
from pathlib import Path
from cube_pipeline import ROOT,symbols

ROWS=512
WORDS=13
LENGTHS=(0,2,8,18,32,98,183,704)


def decode(raw):
    if len(raw)!=4*(8+ROWS*WORDS):raise ValueError('invalid copy benchmark size')
    magic,version,frequency,count,done,errors,ccr,rows=struct.unpack_from('>8I',raw)
    if (magic,version,count,done,errors,rows)!=(0x4d434231,1,ROWS,1,0,ROWS) or not frequency:
        raise ValueError('incomplete or failed copy benchmark')
    result=[];seen=set()
    for index in range(ROWS):
        sr,dr,so,do,length,repetitions,*tail=struct.unpack_from('>13I',raw,32+index*52)
        ticks=tail[:5];checksum,status=tail[5:]
        key=(sr,dr,so,do,length)
        if (sr not in (0,1) or dr not in (0,1) or so>3 or do>3 or length not in LENGTHS
                or repetitions!=(512 if length<=32 else 32) or status or key in seen
                or any(t>=65536 for t in ticks)):
            raise ValueError('invalid copy benchmark case')
        seen.add(key)
        result.append(dict(source_region=sr,destination_region=dr,source_offset=so,
            destination_offset=do,bytes=length,repetitions=repetitions,ticks=ticks,
            median_ticks=statistics.median(ticks),checksum=checksum,
            median_us_per_call=statistics.median(ticks)*1e6/frequency/repetitions))
    return dict(frequency=frequency,ccr=ccr,cases=result,
        scope='Five local FRT intervals per case, including loop/call overhead and interrupts. '
              'Normal Work RAM only. Requested bytes are not bus traffic or hardware certification.')


def compare(before,after):
    paths=[Path(before),Path(after)]
    metadata=[json.loads((path/'capture.json').read_text()) for path in paths]
    for key in ('bios_sha256','probe_sha256','fields'):
        if metadata[0][key]!=metadata[1][key]:raise ValueError('incompatible copy runs: '+key)
    runs=[decode((path/'telemetry.bin').read_bytes()) for path in paths]
    if runs[0]['ccr']!=runs[1]['ccr']:raise ValueError('incompatible copy cache policy')
    pairs=[]
    for old,new in zip(runs[0]['cases'],runs[1]['cases']):
        fields=('source_region','destination_region','source_offset','destination_offset','bytes','checksum')
        if any(old[key]!=new[key] for key in fields):raise ValueError('incompatible copy cases')
        pairs.append(dict(**{k:old[k] for k in fields},before_us=old['median_us_per_call'],
            after_us=new['median_us_per_call'],change_percent=
                (new['median_us_per_call']/old['median_us_per_call']-1)*100
                    if old['median_us_per_call'] else None))
    groups=[]
    for sr in range(2):
        for dr in range(2):
            for length in LENGTHS:
                for category in ('word','halfword','byte'):
                    rows=[r for r in pairs if r['source_region']==sr and r['destination_region']==dr
                          and r['bytes']==length and ('byte' if
                          (r['source_offset']^r['destination_offset'])&1 else 'halfword' if
                          (r['source_offset']^r['destination_offset'])&2 else 'word')==category]
                    old=statistics.median(r['before_us'] for r in rows)
                    new=statistics.median(r['after_us'] for r in rows)
                    groups.append(dict(source_region=sr,destination_region=dr,bytes=length,
                        alignment=category,cases=len(rows),before_median_us=old,after_median_us=new,
                        change_percent=(new/old-1)*100 if old else None))
    return dict(before=str(paths[0]),after=str(paths[1]),frequencies=[r['frequency'] for r in runs],
                cases=pairs,groups=groups,scope=runs[1]['scope'])


def run(a):
    out=a.out.resolve()
    if out.exists() and any(out.iterdir()):raise FileExistsError('use a fresh empty output directory')
    out.mkdir(parents=True,exist_ok=True)
    env=dict(os.environ);env['PATH']=str(a.compiler_dir.resolve())+os.pathsep+env.get('PATH','')
    command=[a.make,'all','EXAMPLE=memory_copy_bench','PYTHON='+sys.executable,'-j4']
    with (out/'build.log').open('w') as log:
        subprocess.run(command,cwd=ROOT,env=env,stdout=log,stderr=subprocess.STDOUT,
                       check=True,timeout=a.timeout)
    for suffix in ('.elf','.map','.app.bin','.iso','.bin','.cue'):
        shutil.copyfile(ROOT/'build'/('memory_copy_bench'+suffix),out/('memory_copy_bench'+suffix))
    syms=symbols(out/'memory_copy_bench.elf',a.compiler_dir/'sh2eb-elf-nm')
    address,size=syms['g_memory_copy_bench']
    command=[str(a.probe.resolve()),'--iso',str(out/'memory_copy_bench.iso'),
        '--bin',str(out/'memory_copy_bench.app.bin'),'--bios',str(a.bios.resolve()),
        '--boot-frames','90','--frames',str(a.fields),'--sh2-cache','on',
        '--slave-reset-entry',hex(syms['saturn_slave_entry'][0]),
        '--dump-mem',f'{address:#x}:{size}:{out}/telemetry.bin','--out',str(out/'ymir.json')]
    (out/'command.json').write_text(json.dumps(command,indent=2)+'\n')
    with (out/'probe.log').open('w') as log:
        subprocess.run(command,cwd=ROOT,stdout=log,stderr=subprocess.STDOUT,check=True,timeout=a.timeout)
    report=decode((out/'telemetry.bin').read_bytes())
    metadata=dict(fields=a.fields,checkout=subprocess.check_output(
        ['git','describe','--always','--dirty'],cwd=ROOT,text=True).strip(),
        bios_sha256=hashlib.sha256(a.bios.read_bytes()).hexdigest(),
        probe_sha256=hashlib.sha256(a.probe.read_bytes()).hexdigest(),
        elf_sha256=hashlib.sha256((out/'memory_copy_bench.elf').read_bytes()).hexdigest())
    (out/'capture.json').write_text(json.dumps(metadata,indent=2)+'\n')
    (out/'analysis.json').write_text(json.dumps(report,indent=2)+'\n')
    print(f"Validated {len(report['cases'])} guarded copy cases; {out}")

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--compare',type=Path,nargs=2);p.add_argument('--out',type=Path)
    p.add_argument('--bios',type=Path);p.add_argument('--fields',type=int,default=2400)
    p.add_argument('--compiler-dir',type=Path,default=Path.home()/'dev/saturn-tools/bin')
    p.add_argument('--probe',type=Path,default=ROOT/'harness/build-macos/probe')
    p.add_argument('--make',default=shutil.which('gmake') or 'make');p.add_argument('--timeout',type=int,default=300)
    a=p.parse_args()
    if a.compare:print(json.dumps(compare(*a.compare),indent=2))
    else:
        if not a.out or not a.bios or a.fields<=0:p.error('require --out, --bios and positive --fields')
        run(a)
