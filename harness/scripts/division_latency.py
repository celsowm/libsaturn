#!/usr/bin/env python3
"""Capture a SH7604 DIVU gap curve; compare with an independent emulator or hardware."""
import argparse,hashlib,json,os,shutil,statistics,struct,subprocess,sys
from pathlib import Path
from cube_pipeline import ROOT,symbols


def decode(raw):
    if len(raw)!=416:raise ValueError('invalid DIVU telemetry size')
    magic,version,frequency,done,errors,tcr,ccr,repetitions=struct.unpack_from('>8I',raw)
    if (magic,version,done,errors,repetitions)!=(0x444c5031,1,1,0,256) or not frequency or tcr&3!=2:
        raise ValueError('failed/incomplete DIVU telemetry or wrong FRT prescaler')
    rows=[]
    for index in range(12):
        bits,gap,*rest=struct.unpack_from('>8I',raw,32+32*index)
        ticks=rest[:5];quotient=rest[5]
        expected_bits=32 if index<6 else 64;expected_gap=(0,8,16,32,48,64)[index%6]
        if (bits,gap)!=(expected_bits,expected_gap) or quotient!=(2726963 if bits==32 else 613566757):
            raise ValueError('invalid DIVU case or quotient')
        if any(t>=65536 for t in ticks):raise ValueError('invalid local FRT interval')
        rows.append(dict(bits=bits,nops=gap,ticks=ticks,median_ticks=statistics.median(ticks),
            quotient=quotient,median_us_per_call=statistics.median(ticks)*1e6/frequency/repetitions))
    curves=[]
    for start in (0,6):
        curve=rows[start:start+6]
        curves.append(dict(bits=curve[0]['bits'],
            early_gap_ticks=curve[2]['median_ticks']-curve[0]['median_ticks'],
            late_gap_ticks=curve[5]['median_ticks']-curve[4]['median_ticks']))
    return dict(frequency=frequency,tcr=tcr,ccr=ccr,repetitions=repetitions,cases=rows,curves=curves,
        scope='Local elapsed FRT intervals include call/loop costs and interrupts. '
              'SH7604 sections 10.3/10.4.1 specify 39-cycle division and access stalls. '
              'Compare early and late NOP gap curves; correct quotients alone do not certify timing.')


def run(a):
    out=a.out.resolve()
    if out.exists() and any(out.iterdir()):raise FileExistsError('use a fresh empty output directory')
    out.mkdir(parents=True,exist_ok=True)
    env=dict(os.environ);env['PATH']=str(a.compiler_dir.resolve())+os.pathsep+env.get('PATH','')
    command=[a.make,'all','EXAMPLE=division_latency_probe','PYTHON='+sys.executable,'-j4']
    with (out/'build.log').open('w') as log:
        subprocess.run(command,cwd=ROOT,env=env,stdout=log,stderr=subprocess.STDOUT,check=True,timeout=a.timeout)
    for suffix in ('.elf','.map','.app.bin','.iso','.bin','.cue'):
        shutil.copyfile(ROOT/'build'/('division_latency_probe'+suffix),out/('division_latency_probe'+suffix))
    syms=symbols(out/'division_latency_probe.elf',a.compiler_dir/'sh2eb-elf-nm')
    address,size=syms['g_division_latency_probe']
    command=[str(a.probe.resolve()),'--iso',str(out/'division_latency_probe.iso'),
        '--bin',str(out/'division_latency_probe.app.bin'),'--bios',str(a.bios.resolve()),
        '--boot-frames','90','--frames',str(a.fields),'--sh2-cache','on',
        '--slave-reset-entry',hex(syms['saturn_slave_entry'][0]),
        '--dump-mem',f'{address:#x}:{size}:{out}/telemetry.bin','--out',str(out/'ymir.json'),
        '--screenshot',f'{a.fields-1}:{out}/screen.png']
    (out/'command.json').write_text(json.dumps(command,indent=2)+'\n')
    with (out/'probe.log').open('w') as log:
        subprocess.run(command,cwd=ROOT,stdout=log,stderr=subprocess.STDOUT,check=True,timeout=a.timeout)
    report=decode((out/'telemetry.bin').read_bytes())
    metadata=dict(checkout=subprocess.check_output(['git','describe','--always','--dirty'],cwd=ROOT,text=True).strip(),
        fields=a.fields,bios_sha256=hashlib.sha256(a.bios.read_bytes()).hexdigest(),
        probe_sha256=hashlib.sha256(a.probe.read_bytes()).hexdigest(),
        elf_sha256=hashlib.sha256((out/'division_latency_probe.elf').read_bytes()).hexdigest())
    (out/'capture.json').write_text(json.dumps(metadata,indent=2)+'\n')
    (out/'analysis.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report,indent=2))

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--bios',type=Path,required=True);p.add_argument('--out',type=Path,required=True)
    p.add_argument('--fields',type=int,default=180);p.add_argument('--timeout',type=int,default=120)
    p.add_argument('--compiler-dir',type=Path,default=Path.home()/'dev/saturn-tools/bin')
    p.add_argument('--probe',type=Path,default=ROOT/'harness/build-macos/probe')
    p.add_argument('--make',default=shutil.which('gmake') or 'make')
    a=p.parse_args()
    if a.fields<=0 or a.timeout<=0:p.error('require positive fields and timeout')
    run(a)
