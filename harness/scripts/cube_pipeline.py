#!/usr/bin/env python3
"""Build/run the procedural cube ladder and decode its bounded FRT telemetry."""
import argparse,hashlib,json,os,shutil,signal,statistics,struct,subprocess
from contextlib import contextmanager
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]
HEADER=('magic version stage divisions hud frequency count cursor serial errors '
        'timeouts field vertices_per_actor faces_per_actor native_faces slave_tasks').split()
FIELDS=('serial field_begin field_end wait begin animation skin geometry flush hud '
        'present work commands faces draw_waits draw_timeouts plot').split()
ROWS=1024
CASES={
    'empty':(0,1),'raw':(1,1),'native':(2,1),'projection':(3,1),
    'scene':(4,1),'texture':(5,1),'skin':(7,1),
    'raw-96':(1,4),'scene-96':(4,4),'texture-96':(5,4),
    'raw-384':(1,8),'scene-384':(4,8),'texture-384':(5,8),
    'raw-864':(1,12),'native-864':(2,12),'projection-864':(3,12),
    'scene-864':(4,12),'texture-864':(5,12),'skin-864':(7,12),
    'paired-864':(8,12),'animated-864':(9,12),
}


@contextmanager
def build_lock():
    """Keep each shared Make alias and its retained copies in one transaction."""
    path=ROOT/'build/locks/cube_pipeline_bench.lock'
    path.parent.mkdir(parents=True,exist_ok=True)
    with path.open('a+b') as handle:
        if os.name=='nt':
            import msvcrt
            handle.write(b'\0');handle.flush();handle.seek(0)
            msvcrt.locking(handle.fileno(),msvcrt.LK_LOCK,1)
        else:
            import fcntl
            fcntl.flock(handle,fcntl.LOCK_EX)
        try:
            yield
        finally:
            if os.name=='nt':
                handle.seek(0);msvcrt.locking(handle.fileno(),msvcrt.LK_UNLCK,1)
            else:
                fcntl.flock(handle,fcntl.LOCK_UN)


def decode(raw,after_field=120):
    if len(raw)!=4*(len(HEADER)+ROWS*len(FIELDS)):raise ValueError('invalid cube telemetry size')
    header=dict(zip(HEADER,struct.unpack_from('>'+str(len(HEADER))+'I',raw)))
    if header['magic']!=0x43334231 or header['version']!=1:raise ValueError('invalid cube telemetry ABI')
    count=header['count'];cursor=header['cursor']
    if not count or cursor!=count%ROWS or count!=header['serial']:
        raise ValueError('incomplete cube telemetry publication')
    if header['errors'] or header['timeouts'] or not header['frequency']:
        raise ValueError('cube guest failed or timer uncalibrated')
    if not 0<=header['stage']<=9 or not 1<=header['divisions']<=12:
        raise ValueError('invalid cube stage/density')
    n=header['divisions']
    if header['vertices_per_actor']!=6*n*n+2 or header['faces_per_actor']!=6*n*n:
        raise ValueError('cube topology does not match density')
    samples=[]
    for index in range(max(0,count-ROWS),count):
        # The next slot may already be getting overwritten by the stopped CPU.
        if count>=ROWS and index%ROWS==cursor:continue
        values=struct.unpack_from('>'+str(len(FIELDS))+'I',raw,
                                  4*len(HEADER)+(index%ROWS)*4*len(FIELDS))
        s=dict(zip(FIELDS,values))
        if s['serial']!=index+1 or s['field_end']<s['field_begin']:
            raise ValueError('incomplete cube sample')
        if s['commands']>=2048 or s['draw_timeouts']:
            raise ValueError('cube command overflow/draw timeout')
        if any(s[key]>s['work'] for key in ('begin','animation','skin','geometry','flush','hud','present','plot')):
            raise ValueError('cube stage exceeds complete work interval')
        if s['field_begin']>=after_field:samples.append(s)
    if len(samples)<2:raise ValueError('insufficient steady cube samples')
    span=samples[-1]['field_begin']-samples[0]['field_begin']
    if span<=0:raise ValueError('non-advancing cube clock')
    costs={}
    for name in ('wait','begin','animation','skin','geometry','flush','hud','present','plot','work'):
        values=sorted(s[name]*1e6/header['frequency'] for s in samples)
        costs[name]=dict(median=statistics.median(values),
                         p95=values[(95*len(values)+99)//100-1],maximum=values[-1])
    return dict(header=header,samples=len(samples),first_field=samples[0]['field_begin'],
                last_field=samples[-1]['field_begin'],measured_fields=span,
                submissions_per_nominal_second=(samples[-1]['serial']-samples[0]['serial'])*60/span,
                stage_us=costs,commands=dict(minimum=min(s['commands'] for s in samples),
                maximum=max(s['commands'] for s in samples)),
                faces=dict(minimum=min(s['faces'] for s in samples),maximum=max(s['faces'] for s in samples)),
                scope='FRT instrumentation; nominal NTSC fields. CPU submissions are not distinct display swaps or physical hardware certification.')


def symbols(elf,nm):
    result={}
    for line in subprocess.check_output([nm,'-S','--defined-only',str(elf)],text=True).splitlines():
        cols=line.split()
        if len(cols)==4:result[cols[3].lstrip('_')]=(int(cols[0],16),int(cols[1],16))
        elif len(cols)==3:result[cols[2].lstrip('_')]=(int(cols[0],16),0)
    return result


def report(paths,compare=()):
    print('| Case | Working RAM | Submit/s | Work ms | Skin ms | Geometry ms | Emit ms | Present ms | Raw plot ms |')
    print('| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |')
    for path in paths:
        data=json.loads(path.read_text())
        for name,case in data['cases'].items():
            costs=case['stage_us']
            values=' | '.join(f"{costs[key]['median']/1000:.3f}" for key in
                             ('work','skin','geometry','flush','present','plot'))
            print(f"| {name} | {case.get('working_ram','low')} | {case['submissions_per_nominal_second']:.2f} | {values} |")
        if compare:
            from PIL import Image,ImageChops
            first=Image.open(path.parent/compare[0]/'screen.png').convert('RGB').crop((0,48,320,192))
            comparisons={}
            for name in compare[1:]:
                other=Image.open(path.parent/name/'screen.png').convert('RGB').crop((0,48,320,192))
                difference=ImageChops.difference(first,other)
                pixels=difference.tobytes()
                comparisons[name]=dict(different_pixels=sum(pixels[i:i+3]!=b'\0\0\0' for i in range(0,len(pixels),3)),
                                       bounds=difference.getbbox())
            print(json.dumps(dict(reference=compare[0],cube_crop=(0,48,320,192),comparisons=comparisons),indent=2))


def record_cd(a):
    """Boot a retained case as a real CD in Mednafen, preserving its movie."""
    case=a.record_cd.resolve();out=a.out.resolve()
    if out.exists() and any(out.iterdir()):raise FileExistsError('use a fresh empty output directory')
    out.mkdir(parents=True,exist_ok=True)
    movie=out/'run.mov';example=a.record_example
    command=[a.mednafen,'-sound','0','-video.driver','softfb','-ss.region_default','na',
             '-ss.bios_na_eu',str(a.bios.resolve()),'-qtrecord',str(movie),
             str(case/(example+'.cue'))]
    with (out/'mednafen.log').open('w') as log:
        process=subprocess.Popen(command,stdout=log,stderr=subprocess.STDOUT)
        try:process.wait(timeout=a.seconds)
        except subprocess.TimeoutExpired:
            process.send_signal(signal.SIGINT)
            try:process.wait(timeout=20)
            except subprocess.TimeoutExpired:
                process.kill();process.wait();raise
    metadata=dict(command=command,returncode=process.returncode,seconds_requested=a.seconds,
                  scope='Actual Mednafen CD boot; inspect movie/screenshot for visual confirmation.',
                  hashes={suffix:hashlib.sha256((case/(example+suffix)).read_bytes()).hexdigest()
                          for suffix in ('.elf','.app.bin','.bin','.cue')})
    (out/'recording.json').write_text(json.dumps(metadata,indent=2)+'\n')
    if process.returncode:raise RuntimeError('Mednafen failed: inspect mednafen.log')
    (out/'ffprobe.json').write_bytes(subprocess.check_output(
        ['ffprobe','-v','quiet','-count_frames','-show_streams','-of','json',str(movie)]))
    subprocess.run(['ffmpeg','-y','-v','error','-sseof','-1','-i',str(movie),
                    '-frames:v','1','-vf','scale=704:480',str(out/'screen.png')],check=True)
    print('Mednafen recording: '+str(out),flush=True)


def run(a):
    out=a.out.resolve()
    if out.exists() and any(out.iterdir()):raise FileExistsError('use a fresh empty output directory')
    out.mkdir(parents=True,exist_ok=True)
    env=dict(os.environ)
    env['LIBSATURN_EXAMPLE_LOCKED']='1'
    compiler=Path(a.compiler_dir).resolve()
    env['PATH']=str(compiler)+os.pathsep+env.get('PATH','')
    nm=str(compiler/'sh2eb-elf-nm')
    results=dict(cases={},completed=False)
    for name in a.cases:
        stage,density=CASES[name];case=out/name;case.mkdir()
        command=['make','all','EXAMPLE=cube_pipeline_bench',f'CUBE_BENCH_STAGE={stage}',
                 f'CUBE_BENCH_DIV={density}',f'CUBE_BENCH_HUD={int(not a.no_hud)}',
                 f'CUBE_BENCH_TIMING_HUD={int(a.timing_hud)}',
                 f'CUBE_BENCH_HIGH_RAM={int(a.high_ram)}',
                 'PYTHON='+str(a.python.resolve()),'-j4']
        with build_lock():
            with (case/'build.log').open('w') as log:
                subprocess.run(command,cwd=ROOT,env=env,stdout=log,stderr=subprocess.STDOUT,
                               check=True,timeout=a.timeout)
            for suffix in ('.elf','.map','.app.bin','.iso','.bin','.cue'):
                shutil.copyfile(ROOT/'build'/('cube_pipeline_bench'+suffix),case/('cube_pipeline_bench'+suffix))
        elf=case/'cube_pipeline_bench.elf';syms=symbols(elf,nm)
        address,size=syms['g_cube_bench'];panic_address,panic_size=syms['g_sat_last_panic']
        ram_address,ram_size=syms['g_cube_bench_high_ram']
        probe=[str(a.probe.resolve()),'--iso',str(case/'cube_pipeline_bench.iso'),
               '--bin',str(case/'cube_pipeline_bench.app.bin'),'--bios',str(a.bios.resolve()),
               '--boot-frames','90','--frames',str(a.frames),'--sh2-cache','on',
               '--out',str(case/'ymir.json'),'--dump-mem',f'{address:#x}:{size}:{case}/telemetry.bin',
               '--dump-mem',f'{panic_address:#x}:{panic_size}:{case}/native-panic.bin',
               '--dump-mem',f'{ram_address:#x}:{ram_size}:{case}/working-ram.bin',
               '--screenshot',f'{a.frames-1}:{case}/screen.png']
        if a.profile:
            probe+=['--debug-profile',str(case/'debug'),'--trace-from',str(a.frames-100),
                    '--trace-frames','6','--trace-capacity','262144']
        (case/'commands.json').write_text(json.dumps([command,probe],indent=2)+'\n')
        with (case/'probe.log').open('w') as log:
            subprocess.run(probe,cwd=ROOT,stdout=log,stderr=subprocess.STDOUT,
                           check=True,timeout=a.timeout)
        panic=(case/'native-panic.bin').read_bytes()
        if len(panic)!=44 or struct.unpack_from('>I',panic)[0]!=0:
            raise ValueError('native cube panic (inspect dump/log/screenshot)')
        report=decode((case/'telemetry.bin').read_bytes(),a.after_field)
        if (case/'working-ram.bin').read_bytes()!=struct.pack('>I',int(a.high_ram)):
            raise ValueError('cube working RAM does not match requested build')
        report['working_ram']='high' if a.high_ram else 'low'
        report['working_buffers']={name:dict(address=syms[name][0],size=syms[name][1])
                                   for name in ('actors','faces','keys','order','worker_faces','worker_keys')
                                   if name in syms}
        ram_start=0x06000000 if a.high_ram else 0x00200000
        if not report['working_buffers'] or any(
                not ram_start<=buffer['address']<buffer['address']+buffer['size']<=ram_start+0x100000
                for buffer in report['working_buffers'].values()):
            raise ValueError('cube ELF buffers do not match requested WRAM placement')
        if report['header']['stage']!=stage or report['header']['divisions']!=density:
            raise ValueError('cube build does not match requested case')
        if report['measured_fields']<a.minimum_fields:raise ValueError('cube measurement window too short')
        report['elf_sha256']=hashlib.sha256(elf.read_bytes()).hexdigest()
        report['execution']=dict(build=command,probe=probe)
        if a.profile:
            profile=json.loads((case/'debug.json').read_text())
            if profile.get('truncated',True) or profile.get('dropped_frames',1):
                raise ValueError('incomplete cube CPU profile')
            for cpu in ('master','slave'):
                if any(profile[cpu].get(key,1) for key in
                       ('dropped_pc_observations','dropped_events','dropped_snapshots')):
                    raise ValueError('dropped cube CPU profile observations')
        report['provenance']={key:hashlib.sha256(path.read_bytes()).hexdigest() for key,path in
            [('probe',a.probe),('bios',a.bios),('source',ROOT/'examples/cube_pipeline_bench/main.c'),
             ('runner',Path(__file__)),('makefile',ROOT/'examples/cube_pipeline_bench/Makefile.inc')]}
        report['provenance']['mesh']=hashlib.sha256((ROOT/'examples/cube_pipeline_bench/mesh_data.h').read_bytes()).hexdigest()
        report['revision']=subprocess.check_output(['git','describe','--always','--dirty'],cwd=ROOT,text=True).strip()
        (case/'analysis.json').write_text(json.dumps(report,indent=2)+'\n')
        results['cases'][name]=report
        (out/'matrix.json').write_text(json.dumps(results,indent=2)+'\n')
        print(f"{name}: {report['submissions_per_nominal_second']:.2f}/s; "
              f"work {report['stage_us']['work']['median']/1000:.3f} ms",flush=True)
    results['completed']=True
    (out/'matrix.json').write_text(json.dumps(results,indent=2)+'\n')


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--out',type=Path);p.add_argument('--bios',type=Path)
    p.add_argument('--report',type=Path,nargs='+',help='summarize retained matrix.json files without rebuilding')
    p.add_argument('--record-cd',type=Path,help='record a retained case directory in Mednafen')
    p.add_argument('--record-example',default='cube_pipeline_bench',help='disc stem for --record-cd')
    p.add_argument('--mednafen',default=shutil.which('mednafen') or 'mednafen')
    p.add_argument('--seconds',type=float,default=30)
    p.add_argument('--compare',nargs='+',default=[],help='compare cube-region screenshots (Pillow required)')
    p.add_argument('--compiler-dir',type=Path,default=Path.home()/'dev/saturn-tools/bin')
    p.add_argument('--python',type=Path,default=Path(os.sys.executable))
    p.add_argument('--probe',type=Path,default=ROOT/'harness/build-macos/probe')
    p.add_argument('--cases',choices=tuple(CASES),nargs='+',default=list(CASES))
    p.add_argument('--frames',type=int,default=1200);p.add_argument('--after-field',type=int,default=120)
    p.add_argument('--minimum-fields',type=int,default=600);p.add_argument('--timeout',type=int,default=300)
    p.add_argument('--no-hud',action='store_true');p.add_argument('--profile',action='store_true')
    p.add_argument('--timing-hud',action='store_true',
                   help='show the measured geometry FRT ticks on the guest HUD')
    p.add_argument('--high-ram',action='store_true',help='place actor/face working buffers in high WRAM')
    a=p.parse_args()
    if a.report:
        report(a.report,a.compare);raise SystemExit(0)
    if not a.out or not a.bios:p.error('--out and --bios are required for a run')
    if a.record_cd:
        if a.seconds<=1:p.error('--seconds must exceed one second')
        record_cd(a);raise SystemExit(0)
    if len(set(a.cases))!=len(a.cases) or a.frames<=a.after_field or a.minimum_fields<1:p.error('invalid cases/window')
    run(a)
