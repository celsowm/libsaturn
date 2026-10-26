#!/usr/bin/env python3
"""Build and capture the exact SH-2 matrix MAC.W experiment; optionally CD boot Mednafen."""
import argparse,hashlib,json,os,shutil,statistics,struct,subprocess,sys,signal
from pathlib import Path
from cube_pipeline import ROOT,symbols


def decode(raw):
    if len(raw)!=672:raise ValueError('invalid matrix telemetry size')
    magic,version,frequency,done,errors,checks,ccr,rows=struct.unpack_from('>8I',raw)
    if (magic,version,done,errors,checks,rows)!=(0x4d4d4231,1,1,0,33792,16) or not frequency:
        raise ValueError('failed/incomplete matrix qualification')
    cases=[]
    for index in range(16):
        kernel,region,chain,repetitions,*rest=struct.unpack_from('>10I',raw,32+index*40)
        ticks,checksum=rest[:5],rest[5]
        expected=(index//4,(index%4)//2,18 if index%2 else 1,8 if index%2 else 64)
        if (kernel,region,chain,repetitions)!=expected or any(t==0 or t>=frequency for t in ticks):
            raise ValueError('invalid matrix case order or clock interval')
        cases.append(dict(kernel=('c64','mac64','c32','mac32')[kernel],region='low' if region else 'high',
            chain=chain,repetitions=repetitions,ticks=ticks,checksum=checksum,
            median_us_per_matrix=statistics.median(ticks)*1e6/frequency/repetitions/chain))
    comparisons=[]
    for index,case in enumerate(cases):
        baseline=cases[index%4]
        if case['checksum']!=baseline['checksum']:raise ValueError('matrix checksum mismatch')
        comparisons.append(dict(kernel=case['kernel'],region=case['region'],chain=case['chain'],
            baseline_us=baseline['median_us_per_matrix'],candidate_us=case['median_us_per_matrix'],
            change_percent=(case['median_us_per_matrix']/baseline['median_us_per_matrix']-1)*100))
    return dict(frequency=frequency,ccr=ccr,checks=checks,cases=cases,comparisons=comparisons,
        scope='Procedural matrix experiment, not consumer performance or hardware qualification. '
        'Includes transpose, SR.S save/clear/restore, floor/clamp, alias publication, loop, checksum '
        'and interrupt cost. 1024 full-range inputs; 9216 exact dots plus 24576 matrix/alias/S tests. '
        'Q12-scale timed inputs, one matrix or 18-link chain, five local FRT intervals. '
        'Ymir DIVU model remains immediate; no physical hardware certification.')


def record_mednafen(a,out):
    directory=out/'mednafen';directory.mkdir()
    command=[a.mednafen,'-sound','0','-video.driver','softfb','-ss.region_default','na',
        '-ss.bios_na_eu',str(a.bios.resolve()),'-qtrecord',str(directory/'run.mov'),
        str(out/'matrix_mac_bench.cue')]
    with (directory/'mednafen.log').open('w') as log:
        process=subprocess.Popen(command,stdout=log,stderr=subprocess.STDOUT)
        try:process.wait(timeout=a.mednafen_seconds)
        except subprocess.TimeoutExpired:
            process.send_signal(signal.SIGINT);process.wait(timeout=20)
    metadata=dict(command=command,returncode=process.returncode,seconds=a.mednafen_seconds,
        scope='Independent CD boot. Inspect final image for checks/errors and displayed raw medians; '
        'a zero process exit alone does not qualify benchmark completion.')
    (directory/'run.json').write_text(json.dumps(metadata,indent=2)+'\n')
    if process.returncode:raise RuntimeError('Mednafen matrix boot failed')
    subprocess.run(['ffmpeg','-y','-v','error','-sseof','-1','-i',str(directory/'run.mov'),
        '-frames:v','1','-vf','scale=704:480',str(directory/'final.png')],check=True)

def run(a):
    out=a.out.resolve()
    if out.exists() and any(out.iterdir()):raise FileExistsError('use a fresh empty output directory')
    out.mkdir(parents=True,exist_ok=True)
    env=dict(os.environ);env['PATH']=str(a.compiler_dir.resolve())+os.pathsep+env.get('PATH','')
    command=[a.make,'all','EXAMPLE=matrix_mac_bench','PYTHON='+sys.executable,'EXAMPLE_INCFLAGS=-'+a.optimization,'-j4']
    with (out/'build.log').open('w') as log:
        subprocess.run(command,cwd=ROOT,env=env,stdout=log,stderr=subprocess.STDOUT,
                       check=True,timeout=a.timeout)
    for suffix in ('.elf','.map','.app.bin','.iso','.bin','.cue'):
        shutil.copyfile(ROOT/'build'/('matrix_mac_bench'+suffix),out/('matrix_mac_bench'+suffix))
    syms=symbols(out/'matrix_mac_bench.elf',a.compiler_dir/'sh2eb-elf-nm')
    address,size=syms['g_matrix_mac_bench']
    panic_address,panic_size=syms['g_sat_last_panic']
    command=[str(a.probe.resolve()),'--iso',str(out/'matrix_mac_bench.iso'),
        '--bin',str(out/'matrix_mac_bench.app.bin'),'--bios',str(a.bios.resolve()),
        '--boot-frames','90','--frames',str(a.fields),'--sh2-cache',a.cache,
        '--slave-reset-entry',hex(syms['saturn_slave_entry'][0]),
        '--dump-mem',f'{address:#x}:{size}:{out}/telemetry.bin',
        '--dump-mem',f'{panic_address:#x}:{panic_size}:{out}/native-panic.bin',
        '--out',str(out/'ymir.json'),'--screenshot',f'{a.fields-1}:{out}/screen.png']
    (out/'command.json').write_text(json.dumps(command,indent=2)+'\n')
    with (out/'probe.log').open('w') as log:
        subprocess.run(command,cwd=ROOT,stdout=log,stderr=subprocess.STDOUT,check=True,timeout=a.timeout)
    panic=(out/'native-panic.bin').read_bytes()
    if len(panic)!=44 or struct.unpack_from('>I',panic)[0]:
        raise ValueError('native matrix benchmark panic')
    report=decode((out/'telemetry.bin').read_bytes())
    metadata=dict(build_command=[a.make,'all','EXAMPLE=matrix_mac_bench',
        'PYTHON='+sys.executable,'EXAMPLE_INCFLAGS=-'+a.optimization,'-j4'],
        source_sha256=hashlib.sha256((ROOT/'examples/matrix_mac_bench/main.c').read_bytes()).hexdigest(),
        compiler_version=subprocess.check_output([str(a.compiler_dir/'sh2eb-elf-gcc'),'--version'],text=True).splitlines()[0],
        fields=a.fields,optimization=a.optimization,cache=a.cache,checkout=subprocess.check_output(
        ['git','describe','--always','--dirty'],cwd=ROOT,text=True).strip(),
        bios_sha256=hashlib.sha256(a.bios.read_bytes()).hexdigest(),
        probe_sha256=hashlib.sha256(a.probe.read_bytes()).hexdigest(),
        elf_sha256=hashlib.sha256((out/'matrix_mac_bench.elf').read_bytes()).hexdigest())
    (out/'capture.json').write_text(json.dumps(metadata,indent=2)+'\n')
    (out/'analysis.json').write_text(json.dumps(report,indent=2)+'\n')
    print(f"Validated {len(report['cases'])} exact matrix cases; {out}")

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--bios',type=Path,required=True);p.add_argument('--out',type=Path,required=True)
    p.add_argument('--fields',type=int,default=600);p.add_argument('--timeout',type=int,default=300)
    p.add_argument('--compiler-dir',type=Path,default=Path.home()/'dev/saturn-tools/bin')
    p.add_argument('--probe',type=Path,default=ROOT/'harness/build-macos/probe')
    p.add_argument('--optimization',choices=('O2','O3'),default='O2')
    p.add_argument('--cache',choices=('on','off'),default='on')
    p.add_argument('--make',default=shutil.which('gmake') or 'make')
    p.add_argument('--mednafen',default=shutil.which('mednafen') or 'mednafen')
    p.add_argument('--mednafen-seconds',type=float,default=0)
    a=p.parse_args()
    if a.fields<=0 or a.timeout<=0 or a.mednafen_seconds<0:p.error('invalid duration')
    run(a)
    if a.mednafen_seconds:record_mednafen(a,a.out.resolve())
