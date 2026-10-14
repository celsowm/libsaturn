"""Prepare local hardware packages and an explicitly firmware-free application ZIP."""
import argparse
import csv
import json
import os
import shutil
import subprocess
import zipfile
from pathlib import Path
from boot_image import digest


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root',type=Path,required=True)
    args=parser.parse_args()
    root=args.root
    reports=[]
    shared=root/'applications-only'
    shared.mkdir(exist_ok=True)
    applications={}
    for folder in sorted(root.iterdir()):
        report_path=folder/'boot-report.json'
        if not report_path.exists():continue
        report=json.loads(report_path.read_text(encoding='utf-8'))
        profile=folder.name.split('-')[0]
        app=folder/'optical_emulators_tester.app.bin'
        sha=digest(app.read_bytes())
        if profile in applications and applications[profile]!=sha:
            raise ValueError(f'{profile}: IP-only builds changed the application!')
        applications[profile]=sha
        shutil.copyfile(app,shared/(profile+'.app.bin'))
        files={p.name:dict(bytes=p.stat().st_size,sha256=digest(p.read_bytes()))
               for p in sorted(folder.iterdir()) if p.suffix.lower() in ('.bin','.iso','.cue','.elf','.map')}
        reports.append(dict(name=folder.name,files=files,boot=report,
                            physical_satiator='pending',physical_saroo='pending'))
    manifest=dict(schema_version=1,disc_redistribution='not_cleared; local diagnostic outputs only',
                  applications_identical_across_boot_profiles=True,packages=reports)
    repository=Path(__file__).resolve().parents[1]
    paths=subprocess.check_output(['git','ls-files','-z','--cached','--others','--exclude-standard'],
                                  cwd=repository).decode('utf-8').split('\0')
    inputs={}
    for name in sorted(set(paths)):
        if not name:continue
        path=repository/name
        relevant=name.startswith(('src/','include/','tools/','cmake/','examples/optical_emulators_tester/','harness/','tests/'))
        if path.is_file() and (relevant or name in ('Makefile','CMakeLists.txt','conanfile.py','build-example.ps1')):
            if path.suffix.lower() in ('.c','.cpp','.h','.hpp','.s','.py','.ps1','.cmake','.txt') or path.name=='Makefile':
                inputs[name]=digest(path.read_bytes())
    manifest['source_revision']=subprocess.check_output(['git','rev-parse','HEAD'],cwd=repository,text=True).strip()
    manifest['source_dirty']=bool(subprocess.check_output(['git','status','--porcelain'],cwd=repository))
    manifest['source_files_sha256']=inputs
    manifest['source_date_epoch']=int(os.environ.get('SOURCE_DATE_EPOCH','1791936000'))
    payload_manifest=repository/'build/native-boot/payloads.json'
    if payload_manifest.exists():manifest['native_payload_policy']=json.loads(payload_manifest.read_text(encoding='utf-8'))
    (root/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8')
    readme=Path(__file__).resolve().parents[1]/'examples/optical_emulators_tester/README.md'
    if readme.exists():
        shutil.copyfile(readme,root/'README.md');shutil.copyfile(readme,shared/'README.md')
    columns=['package','image_sha256','console_revision','bios_region_revision','video_standard',
             'ode','firmware','menu_version','sd_filesystem','launch_mode','cold_or_reset',
             'attempt','visible_frame','last_stage','result','hirq','cr1','cr2','cr3','cr4',
             'lba','bytes_requested','bytes_transferred','elapsed_ms','timeouts','evidence','notes']
    with (root/'hardware-results.csv').open('w',newline='',encoding='utf-8') as f:
        writer=csv.writer(f);writer.writerow(columns)
        for r in reports:writer.writerow([r['name'],r['files']['optical_emulators_tester.iso']['sha256']]+['']*(len(columns)-2))
        for variants in sorted(root.glob('header-variants-*/variants.json')):
            for v in json.loads(variants.read_text(encoding='utf-8')):
                writer.writerow([variants.parent.name+'/'+v['name'],v['report']['iso_sha256']]+['']*(len(columns)-2))
    with zipfile.ZipFile(root/'applications-only.zip','w',zipfile.ZIP_DEFLATED) as archive:
        for path in sorted(shared.iterdir()):archive.write(path,path.name)
    print(f'[optical-package] {len(reports)} local disc packages; application hashes agree across IP profiles')


if __name__=='__main__':
    main()
